#pragma once

#include <astral/assets/model.hpp>
#include <astral/render/material_file.hpp>
#include <astral/render/renderer.hpp>

#include <algorithm>
#include <array>

namespace astral::detail {

inline constexpr ModelSampler default_model_sampler{};

inline unsigned imported_surface_key(const ModelMaterial& material) {
    const bool custom_sampler = std::any_of(material.samplers.begin(), material.samplers.end(),
                                            [](const auto& sampler) { return sampler != default_model_sampler; });
    return (custom_sampler ? 16u : 0u) | static_cast<unsigned>(material.alpha_mode) | (material.double_sided ? 4u : 0u) |
           (material.occlusion_image >= 0 ? 8u : 0u);
}

inline constexpr std::string_view imported_sampler_source = R"(
uniform vec4 base_sampling = vec4(1.0, 1.0, 5.0, 1.0);
uniform vec4 mr_sampling = vec4(1.0, 1.0, 5.0, 1.0);
uniform vec4 normal_sampling = vec4(1.0, 1.0, 5.0, 1.0);
uniform vec4 emission_sampling = vec4(1.0, 1.0, 5.0, 1.0);
uniform vec4 occlusion_sampling = vec4(1.0, 1.0, 5.0, 1.0);

int gltf_address(int at, int size, int mode) {
    if (mode == 0) return clamp(at, 0, size - 1);
    int period = mode == 2 ? 2 * size : size;
    int index = (at % period + period) % period;
    return index < size ? index : period - 1 - index;
}

vec4 gltf_texel(uint img, ivec2 p, ivec2 size, ivec2 wrap_mode, int mip) {
    ivec2 index = ivec2(gltf_address(p.x, size.x, wrap_mode.x), gltf_address(p.y, size.y, wrap_mode.y));
    return texelFetch(sampler2D(astral_textures[nonuniformEXT(img)], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]), index, mip);
}

vec4 gltf_level(uint img, vec2 uv, ivec2 wrap_mode, int mip, bool linear_filter) {
    ivec2 size = max(textureSize(sampler2D(astral_textures[nonuniformEXT(img)], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]), mip), ivec2(1));
    vec2 texel = uv * vec2(size) - (linear_filter ? 0.5 : 0.0);
    ivec2 base = ivec2(floor(texel));
    vec4 c00 = gltf_texel(img, base, size, wrap_mode, mip);
    if (!linear_filter) return c00;
    vec2 fraction = texel - vec2(base);
    vec4 top = mix(c00, gltf_texel(img, base + ivec2(1, 0), size, wrap_mode, mip), fraction.x);
    vec4 bottom = mix(gltf_texel(img, base + ivec2(0, 1), size, wrap_mode, mip),
                      gltf_texel(img, base + ivec2(1, 1), size, wrap_mode, mip), fraction.x);
    return mix(top, bottom, fraction.y);
}

vec4 gltf_sample(uint img, vec2 uv, vec2 uv_dx, vec2 uv_dy, vec4 config) {
    ivec2 wrap_mode = ivec2(config.xy);
#ifdef ASTRAL_RT_CHUNK
    return gltf_level(img, uv, wrap_mode, 0, config.w != 0.0);
#else
    vec2 extent = vec2(textureSize(sampler2D(astral_textures[nonuniformEXT(img)], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP]), 0));
    float footprint = max(length(uv_dx * extent), length(uv_dy * extent));
    int mode = int(config.z);
    bool linear_filter = footprint <= 1.0 ? (config.w != 0.0) : ((mode & 1) != 0);
    int mip0 = 0, mip1 = 0;
    float mip_t = 0.0;
    if (footprint > 1.0 && mode >= 2) {
        int max_mip = max(textureQueryLevels(sampler2D(astral_textures[nonuniformEXT(img)], astral_samplers[ASTRAL_SAMPLER_NEAREST_CLAMP])) - 1, 0);
        float lod = clamp(log2(footprint), 0.0, float(max_mip));
        if (mode < 4) {
            mip0 = int(floor(lod + 0.5));
            mip1 = mip0;
        } else {
            mip0 = int(floor(lod));
            mip1 = min(mip0 + 1, max_mip);
            mip_t = fract(lod);
        }
    }
    vec4 a = gltf_level(img, uv, wrap_mode, mip0, linear_filter);
    return mip1 == mip0 ? a : mix(a, gltf_level(img, uv, wrap_mode, mip1, linear_filter), mip_t);
#endif
}
)";

inline std::string imported_surface_source(const ModelMaterial& material) {
    std::string source = "shader_type surface;\n";
    if (material.alpha_mode == ModelAlphaMode::Mask) source += "render_mode alpha_test;\n";
    if (material.alpha_mode == ModelAlphaMode::Blend) source += "render_mode blend_mix;\n";
    if (material.double_sided) source += "render_mode cull_disabled;\n";
    const bool custom_sampler = (imported_surface_key(material) & 16u) != 0;
    if (custom_sampler) source += imported_sampler_source;
    source += R"(
uniform vec4 base_color = vec4(1.0);
uniform vec3 emission_factor = vec3(0.0);
uniform float roughness_factor = 1.0;
uniform float metallic_factor = 1.0;
uniform float normal_scale = 1.0;
uniform float alpha_cutoff = 0.5;
uniform float occlusion_strength = 1.0;
uniform sampler2D base_color_texture : source_color, repeat;
uniform sampler2D metallic_roughness_texture : repeat;
uniform sampler2D normal_texture : hint_normal, repeat;
uniform sampler2D emissive_texture : source_color, repeat;
uniform sampler2D occlusion_texture : repeat;
void surface() {
    vec4 base = base_color * texture(base_color_texture, UV);
    vec4 mr = texture(metallic_roughness_texture, UV);
    vec2 n = texture(normal_texture, UV).xy * 2.0 - 1.0;
    float nz = sqrt(max(0.0, 1.0 - dot(n, n)));
    ALBEDO = base.rgb;
    ALPHA = base.a;
    ALPHA_SCISSOR = alpha_cutoff;
    METALLIC = metallic_factor * mr.b;
    ROUGHNESS = roughness_factor * mr.g;
    NORMAL_MAP = normalize(vec3(n * normal_scale, max(nz, 0.0001))) * 0.5 + 0.5;
    EMISSION = emission_factor * texture(emissive_texture, UV).rgb;
    AO = mix(1.0, texture(occlusion_texture, UV).r, occlusion_strength);
}
)";
    if (custom_sampler) {
        const std::pair<std::string_view, std::string_view> names[] = {
            {"base_color_texture", "base_sampling"}, {"metallic_roughness_texture", "mr_sampling"},
            {"normal_texture", "normal_sampling"}, {"emissive_texture", "emission_sampling"},
            {"occlusion_texture", "occlusion_sampling"},
        };
        for (const auto& [name, config] : names) {
            const std::string call = "texture(" + std::string(name) + ", UV)";
            const std::string replacement = "gltf_sample(astral_image_" + std::string(name) + ", UV, dFdx(UV), dFdy(UV), " +
                                            std::string(config) + ")";
            if (const auto at = source.find(call); at != std::string::npos) source.replace(at, call.size(), replacement);
        }
    }
    return source;
}

inline MaterialFile imported_surface_values(const ModelMaterial& m) {
    const auto c = m.base_color;
    const auto e = m.emissive;
    MaterialFile result;
    result.values = {
        {"base_color", Json::array({c.x, c.y, c.z, c.w})},
        {"emission_factor", Json::array({e.x, e.y, e.z})},
        {"roughness_factor", m.roughness}, {"metallic_factor", m.metallic}, {"normal_scale", m.normal_scale},
        {"alpha_cutoff", m.alpha_cutoff}, {"occlusion_strength", m.occlusion_strength},
    };
    constexpr std::string_view names[] = {"base_sampling", "mr_sampling", "normal_sampling", "emission_sampling",
                                          "occlusion_sampling"};
    if ((imported_surface_key(m) & 16u) != 0) {
        for (std::size_t i = 0; i < m.samplers.size(); ++i) {
            const auto& sampler = m.samplers[i];
            result.values.emplace_back(names[i], Json::array({sampler.wrap_u, sampler.wrap_v, sampler.min_filter,
                                                              sampler.mag_linear ? 1 : 0}));
        }
    }
    return result;
}

inline std::array<std::pair<std::string_view, int>, 5> imported_surface_images(const ModelMaterial& m) {
    return {{{"base_color_texture", m.base_color_image}, {"metallic_roughness_texture", m.metallic_roughness_image},
             {"normal_texture", m.normal_image}, {"emissive_texture", m.emissive_image},
             {"occlusion_texture", m.occlusion_image}}};
}

} // namespace astral::detail
