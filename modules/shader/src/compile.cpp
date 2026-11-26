#include <astral/shader/compile.hpp>

#include "sha256.hpp"

#include <astral/core/jobs.hpp>

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

#include <cstdlib>
#include <mutex>
#include <sstream>

namespace astral {

namespace {

void initialize_once() {
    static std::once_flag once;
    std::call_once(once, [] {
        glslang::InitializeProcess();
        std::atexit([] { glslang::FinalizeProcess(); });
    });
}

EShLanguage language(ShaderStage stage) {
    switch (stage) {
    case ShaderStage::Vertex: return EShLangVertex;
    case ShaderStage::Fragment: return EShLangFragment;
    case ShaderStage::Compute: return EShLangCompute;
    }
    return EShLangFragment;
}

class Includer final : public glslang::TShader::Includer {
public:
    explicit Includer(std::span<const GlslInclude> files) : files_(files) {}
    IncludeResult* includeSystem(const char* name, const char*, size_t) override { return find(name); }
    IncludeResult* includeLocal(const char* name, const char*, size_t) override { return find(name); }
    void releaseInclude(IncludeResult* result) override { delete result; }

private:
    IncludeResult* find(const char* name) {
        for (const GlslInclude& f : files_) {
            if (f.name == name) return new IncludeResult(f.name, f.text.data(), f.text.size(), nullptr);
        }
        return nullptr;
    }
    std::span<const GlslInclude> files_;
};

/// "ERROR: name:12: 'x' : undeclared identifier" → {12, "'x' : undeclared identifier"}.
void parse_log(const std::string& log, std::vector<ShaderError>& errors) {
    std::istringstream in(log);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.starts_with("ERROR: ")) continue;
        std::string rest = line.substr(7);
        const std::size_t a = rest.find(':');
        const std::size_t b = a == std::string::npos ? std::string::npos : rest.find(':', a + 1);
        std::uint32_t number = 0;
        std::string message = rest;
        if (b != std::string::npos) {
            const std::string digits = rest.substr(a + 1, b - a - 1);
            char* end = nullptr;
            const unsigned long n = std::strtoul(digits.c_str(), &end, 10);
            if (!digits.empty() && end && *end == '\0') {
                number = static_cast<std::uint32_t>(n);
                message = rest.substr(b + 1);
                while (!message.empty() && message.front() == ' ') message.erase(0, 1);
            }
        }
        if (number == 0 && message.find("compilation errors") != std::string::npos) continue;
        errors.push_back({number, message});
    }
}

/// Everything besides the source text that changes the SPIR-V glslang produces. Bump the tag when
/// compile_uncached changes how it drives the compiler.
std::string compiler_identity() {
    const glslang::Version v = glslang::GetVersion();
    return "astral-spirv-1|glslang " + std::to_string(v.major) + "." + std::to_string(v.minor) + "." +
           std::to_string(v.patch) + "|glsl460|vulkan1.3|spirv1.5|noopt|nodebug";
}

detail::Sha256Digest digest_includes(std::span<const GlslInclude> includes) {
    detail::Sha256 sha;
    sha.update_u32(static_cast<std::uint32_t>(includes.size()));
    for (const GlslInclude& file : includes) {
        sha.update_field(file.name);
        sha.update_field(file.text);
    }
    return sha.finish();
}

SpirvCacheKey make_key(std::string_view source, ShaderStage stage, std::string_view name,
                       const detail::Sha256Digest& includes) {
    static const std::string identity = compiler_identity();
    detail::Sha256 sha;
    sha.update_field(identity);
    sha.update_u32(static_cast<std::uint32_t>(stage));
    sha.update_field(name);
    sha.update_field(source);
    sha.update(std::span<const std::uint8_t>(includes));
    return {sha.finish()};
}

GlslCompile compile_uncached(std::string_view source, ShaderStage stage, std::span<const GlslInclude> includes,
                             std::string_view name) {
    initialize_once();
    GlslCompile out;
    const EShLanguage lang = language(stage);
    glslang::TShader shader(lang);
    const std::string text(source);
    const std::string file(name);
    const char* strings[] = {text.c_str()};
    const char* names[] = {file.c_str()};
    const int lengths[] = {static_cast<int>(text.size())};
    shader.setStringsWithLengthsAndNames(strings, lengths, names, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, lang, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
    const auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
    Includer includer(includes);
    if (!shader.parse(GetDefaultResources(), 460, false, messages, includer)) {
        out.log = shader.getInfoLog();
        parse_log(out.log, out.errors);
        if (out.errors.empty()) out.errors.push_back({0, out.log});
        return out;
    }
    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(messages)) {
        out.log = program.getInfoLog();
        parse_log(out.log, out.errors);
        if (out.errors.empty()) out.errors.push_back({0, out.log});
        return out;
    }
    glslang::SpvOptions options;
    options.generateDebugInfo = false;
    options.disableOptimizer = true;
    glslang::GlslangToSpv(*program.getIntermediate(lang), out.spirv, &options);
    out.log = shader.getInfoLog();
    return out;
}

/// compile_uncached behind the cache; `includes_digest` is only read (and so only computed) with a cache.
GlslCompile compile_with_cache(std::string_view source, ShaderStage stage, std::span<const GlslInclude> includes,
                               std::string_view name, SpirvCache* cache, const detail::Sha256Digest& includes_digest) {
    SpirvCacheKey key;
    if (cache) {
        key = make_key(source, stage, name, includes_digest);
        GlslCompile cached;
        if (cache->lookup(key, cached.spirv)) {
            cached.from_cache = true;
            return cached;
        }
    }
    GlslCompile out = compile_uncached(source, stage, includes, name);
    if (cache && out.ok()) cache->store(key, out.spirv);
    return out;
}

} // namespace

GlslCompile compile_glsl(std::string_view source, ShaderStage stage, std::span<const GlslInclude> includes, std::string_view name,
                         const ShaderCompileServices& services) {
    const detail::Sha256Digest digest = services.cache ? digest_includes(includes) : detail::Sha256Digest{};
    return compile_with_cache(source, stage, includes, name, services.cache, digest);
}

std::vector<GlslCompile> compile_glsl_batch(std::span<const GlslRequest> requests, std::span<const GlslInclude> includes,
                                            const ShaderCompileServices& services) {
    std::vector<GlslCompile> results(requests.size());
    const detail::Sha256Digest digest = services.cache ? digest_includes(includes) : detail::Sha256Digest{};
    const auto run = [&](std::size_t i) {
        const GlslRequest& r = requests[i];
        results[i] = compile_with_cache(r.source, r.stage, includes, r.name, services.cache, digest);
    };
    if (services.jobs && requests.size() > 1) {
        JobCounter counter;
        for (std::size_t i = 0; i < requests.size(); ++i) services.jobs->submit(counter, [&run, i] { run(i); });
        services.jobs->wait(counter);
    } else {
        for (std::size_t i = 0; i < requests.size(); ++i) run(i);
    }
    return results;
}

} // namespace astral
