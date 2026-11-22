#pragma once
// .material files (ADR-0022): a shader plus values for its uniforms, optionally on top of a parent
// material (only the differences are written, like Unreal's material instances).
//
//   { "format": "astral.material", "version": 1,
//     "shader": "shaders/foliage.ashader",              // or inherited from "parent"
//     "parent": "materials/foliage_base.material",
//     "values": { "tint": [0.6, 0.9, 0.5, 1], "albedo": "textures/leaf.png", "sway": 0.45 } }
//
// Pure data (no file system, no GPU): the caller supplies a reader and turns the result into a
// Renderer material.

#include <astral/core/json.hpp>
#include <astral/shader/ashader.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace astral {

/// Reads and parses a document by asset path; nullopt (with `error`) if it cannot.
using MaterialReader = std::function<std::optional<Json>(std::string_view path, std::string* error)>;

/// A material with its parent chain applied.
struct MaterialFile {
    std::string shader;                                ///< Asset path of the .ashader.
    std::vector<std::pair<std::string, Json>> values;  ///< Uniform name → value; the child's wins.
    std::vector<std::string> dependencies;             ///< This file, then each parent: what hot reload watches.
    std::vector<std::string> errors;
    [[nodiscard]] bool ok() const noexcept { return errors.empty() && !shader.empty(); }
    [[nodiscard]] const Json* value(std::string_view name) const noexcept;
};

/// Follows the "parent" chain (at most 16 deep; cycles are errors).
[[nodiscard]] MaterialFile resolve_material(std::string_view path, const MaterialReader& read);

/// A material's values laid out for a shader: the parameter block (layout.words floats) and the
/// textures to load into it.
struct UniformValues {
    std::vector<float> params;
    struct Texture {
        std::uint32_t offset = 0;
        std::string path;
        bool srgb = false;   ///< source_color
        bool normal = false; ///< hint_normal
    };
    std::vector<Texture> textures;
    std::vector<std::string> warnings; ///< Unknown names and values of the wrong type (defaults are used).
};

[[nodiscard]] UniformValues evaluate_uniforms(const ShaderSource& shader, const UniformLayout& layout, const MaterialFile& material);

} // namespace astral
