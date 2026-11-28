#pragma once
// type "layered" materials (ADR-0055 D7): a stack of materials blended by masks, written as a .material and turned
// into ONE generated .ashader, so the result is compiled, cached, hot reloaded and ray traced like any other shader.
//
//   { "format": "astral.material", "version": 1, "type": "layered",
//     "layers": [
//       { "material": "materials/rock.material" },                                   // the base: no mask
//       { "material": "materials/moss.material",
//         "mask": { "texture": "textures/moss_mask.png", "channel": "r", "slope": [0.6, 0.9],
//                   "height_world": [0, 8], "noise": { "scale": 2, "amount": 0.4 },
//                   "value": 1.0, "invert": false, "contrast": 0.3, "height_blend": true, "depth": 0.2 },
//         "height": { "texture": "textures/moss_height.png", "channel": "g", "scale": 1.0 } } ],
//     "values": { "layer1_mask": 0.8 } }                                              // overrides: layer<N>_<uniform>
//
// A layer is a standard PBR material or a surface .ashader material (its parents and values apply). Per layer only
// ALBEDO, ROUGHNESS, METALLIC, EMISSION, AO and NORMAL_MAP are blended; vertex(), varyings and the other built-ins
// (clearcoat, wetness, ...) of a layer shader are not used. Mask factors multiply; at most `max_layers` layers.

#include <astral/core/json.hpp>
#include <astral/render/material_file.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace astral {

inline constexpr std::size_t max_material_layers = 8;

/// Reads a text asset (a layer's .ashader); nullopt (with `error`) if it cannot.
using TextReader = std::function<std::optional<std::string>(std::string_view path, std::string* error)>;

struct LayeredShader {
    std::string source;                    ///< The generated .ashader text (the single source of truth, diffable).
    std::vector<std::string> dependencies; ///< Every layer material, parent and shader read: what hot reload watches.
    std::vector<std::string> errors;
    [[nodiscard]] bool ok() const noexcept { return errors.empty() && !source.empty(); }
};

/// Generates the shader of a layered material. `shading_model` and `render_modes` are the .ashader header values to
/// use ("" / empty = defaults). Pure: all file access goes through the two readers.
[[nodiscard]] LayeredShader compose_layered_shader(const Json& layers, std::string_view shading_model,
                                                   const std::vector<std::string>& render_modes,
                                                   const MaterialReader& read_material, const TextReader& read_text);

} // namespace astral
