#pragma once
// GLSL -> SPIR-V at run time (glslang), for .ashader pipelines and hot reload (ADR-0022). Thread-safe:
// may be called from jobs. Targets Vulkan 1.3 / SPIR-V 1.5 (1.6 would turn `discard` into demote-to-helper,
// an optional device feature).

#include <astral/shader/ashader.hpp>

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

struct GlslCompile {
    std::vector<std::uint32_t> spirv;
    std::vector<ShaderError> errors; ///< Lines as the compiler saw them (#line directives respected).
    std::string log;                 ///< Full compiler output.
    [[nodiscard]] bool ok() const noexcept { return errors.empty() && !spirv.empty(); }
};

[[nodiscard]] GlslCompile compile_glsl(std::string_view source, ShaderStage stage, std::span<const GlslInclude> includes = {},
                                       std::string_view name = "shader");

} // namespace astral
