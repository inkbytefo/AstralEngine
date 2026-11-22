#pragma once
// The .ashader language (ADR-0022): a small Godot-like header (shader_type, render_mode, uniforms with
// hints, varyings) over a plain GLSL body. This header only parses; the renderer turns a parsed shader
// into its pipelines. The body is kept verbatim with header statements blanked, so line numbers match
// the file (generated code uses #line, compiler errors point at the .ashader line).

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace astral {

enum class ShaderType : std::uint8_t { Surface, Particle, Volume, Post, Sky, Ui };
enum class UniformType : std::uint8_t { Float, Vec2, Vec3, Vec4, Int, Bool, Sampler2D };
enum class TextureFilter : std::uint8_t { Linear, LinearMipmap, Nearest, NearestMipmap };

struct UniformRange {
    float min = 0.0f;
    float max = 1.0f;
    float step = 0.0f; ///< 0: continuous
};

struct UniformHints {
    bool source_color = false; ///< Colour (sRGB texture / colour picker).
    bool hint_normal = false;  ///< Normal map texture.
    std::optional<UniformRange> range;
    TextureFilter filter = TextureFilter::LinearMipmap;
    bool repeat = true;
};

struct ShaderUniform {
    std::string name;
    UniformType type = UniformType::Float;
    UniformHints hints;
    std::vector<float> default_value; ///< Components (vec4(1.0) → 4 ones); empty = zeros / no texture.
    std::uint32_t line = 0;
};

struct ShaderVarying {
    std::string type; ///< GLSL type as written (float, vec2, vec3, vec4).
    std::string name;
};

struct ShaderSource {
    ShaderType type = ShaderType::Surface;
    std::vector<std::string> render_modes;
    std::vector<ShaderUniform> uniforms;
    std::vector<ShaderVarying> varyings;
    std::vector<std::string> functions; ///< Names of top-level functions defined in the body.
    /// [begin, end) of each function in `body` (same order as `functions`).
    std::vector<std::pair<std::size_t, std::size_t>> function_ranges;
    std::string body;                   ///< GLSL, header statements blanked, same lines as the file.

    /// The body with every definition of `function` blanked (lines kept): vertex() is left out of the
    /// fragment stage and surface() out of the vertex stage.
    [[nodiscard]] std::string body_without(std::string_view function) const;

    [[nodiscard]] bool has_render_mode(std::string_view mode) const noexcept;
    [[nodiscard]] bool has_function(std::string_view name) const noexcept;
};

struct ShaderError {
    std::uint32_t line = 0; ///< 1-based line in the .ashader file.
    std::string message;
};

struct ShaderParse {
    ShaderSource source;
    std::vector<ShaderError> errors;
};

/// Parses a .ashader file. Never throws; every problem is reported with its line.
[[nodiscard]] ShaderParse parse_ashader(std::string_view text);

/// The render modes a shader_type accepts (for validation and tools).
[[nodiscard]] std::vector<std::string_view> known_render_modes(ShaderType type);
[[nodiscard]] std::string_view to_string(ShaderType type) noexcept;
[[nodiscard]] std::string_view to_string(UniformType type) noexcept;

/// Where each uniform lives in the material's parameter block (std430, 4-byte words). Textures take
/// one word (bindless index); vec3 aligns like vec4 but the next scalar may pack into its 4th word.
struct UniformLayout {
    std::vector<std::uint32_t> offsets; ///< Per uniform, in words.
    std::uint32_t words = 0;            ///< Total size rounded up to 4 words.
};
[[nodiscard]] UniformLayout layout_uniforms(const ShaderSource& source);

} // namespace astral
