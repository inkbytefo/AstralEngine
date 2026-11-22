#include <astral/shader/compile.hpp>

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

} // namespace

GlslCompile compile_glsl(std::string_view source, ShaderStage stage, std::span<const GlslInclude> includes, std::string_view name) {
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

} // namespace astral
