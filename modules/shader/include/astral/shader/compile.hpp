#pragma once
// GLSL -> SPIR-V at run time (glslang), for .ashader pipelines and hot reload (ADR-0022). Thread-safe:
// may be called from jobs. Targets Vulkan 1.3 / SPIR-V 1.5 (1.6 would turn `discard` into demote-to-helper,
// an optional device feature).
//
// glslang is slow (seconds per large shader in a Debug build), so two optional services cut start-up time:
// a SpirvCache (compile once per machine) and a JobSystem (compile the independent stages of one request
// concurrently). Both give byte-identical SPIR-V to a plain serial compile.

#include <astral/shader/ashader.hpp>
#include <astral/shader/spirv_cache.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace astral {

enum class ShaderStage : std::uint8_t { Vertex, Fragment, Compute };

/// A file reachable with #include <name> (engine headers, generated code).
struct GlslInclude {
    std::string name;
    std::string text;
};

class JobSystem;

/// Optional helpers for compile_glsl and everything built on it. Both pointers are borrowed (null = off) and
/// must outlive the call; there is no global default.
struct ShaderCompileServices {
    SpirvCache* cache = nullptr;
    JobSystem* jobs = nullptr;
};

struct GlslCompile {
    std::vector<std::uint32_t> spirv;
    std::vector<ShaderError> errors; ///< Lines as the compiler saw them (#line directives respected).
    std::string log;                 ///< Full compiler output (empty when the SPIR-V came from the cache).
    bool from_cache = false;         ///< Answered by SpirvCache: glslang did not run.
    [[nodiscard]] bool ok() const noexcept { return errors.empty() && !spirv.empty(); }
};

[[nodiscard]] GlslCompile compile_glsl(std::string_view source, ShaderStage stage, std::span<const GlslInclude> includes = {},
                                       std::string_view name = "shader", const ShaderCompileServices& services = {});

/// One source of a compile_glsl_batch.
struct GlslRequest {
    std::string source;
    ShaderStage stage = ShaderStage::Fragment;
    std::string name = "shader";
};

/// Compiles independent sources that share one include set. Results come back in request order and equal
/// what compile_glsl returns for each request alone; with services.jobs they run concurrently.
[[nodiscard]] std::vector<GlslCompile> compile_glsl_batch(std::span<const GlslRequest> requests,
                                                          std::span<const GlslInclude> includes = {},
                                                          const ShaderCompileServices& services = {});

} // namespace astral
