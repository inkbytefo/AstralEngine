#pragma once

#include <astral/render/surface_shader.hpp>
#include <astral/shader/compile.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace astral::embedded {
extern const std::uint8_t astral_bindless_glsl[];
extern const std::size_t astral_bindless_glsl_size;
extern const std::uint8_t astral_scene_glsl[];
extern const std::size_t astral_scene_glsl_size;
extern const std::uint8_t astral_color_glsl[];
extern const std::size_t astral_color_glsl_size;
extern const std::uint8_t astral_brdf_glsl[];
extern const std::size_t astral_brdf_glsl_size;
extern const std::uint8_t astral_bsdf_glsl[];
extern const std::size_t astral_bsdf_glsl_size;
extern const std::uint8_t astral_rt_glsl[];
extern const std::size_t astral_rt_glsl_size;
extern const std::uint8_t astral_restir_glsl[];
extern const std::size_t astral_restir_glsl_size;
extern const std::uint8_t astral_primary_glsl[];
extern const std::size_t astral_primary_glsl_size;
extern const std::uint8_t astral_sdf_glsl[];
extern const std::size_t astral_sdf_glsl_size;
extern const std::uint8_t astral_rt_lighting_comp[];
extern const std::size_t astral_rt_lighting_comp_size;
extern const std::uint8_t astral_ddgi_trace_comp[];
extern const std::size_t astral_ddgi_trace_comp_size;
extern const std::uint8_t astral_restir_di_temporal_comp[];
extern const std::size_t astral_restir_di_temporal_comp_size;
extern const std::uint8_t astral_restir_di_spatial_comp[];
extern const std::size_t astral_restir_di_spatial_comp_size;
extern const std::uint8_t astral_particle_frag[];
extern const std::size_t astral_particle_frag_size;
extern const std::uint8_t astral_particle_vert[];
extern const std::size_t astral_particle_vert_size;
extern const std::uint8_t astral_path_tracer_comp[];
extern const std::size_t astral_path_tracer_comp_size;
} // namespace astral::embedded

namespace astral::detail {

inline constexpr const char* kSurfaceBuiltinsDecl = R"(vec2 UV; vec3 NORMAL; vec3 WORLD_POSITION; vec3 VIEW_DIR; bool FRONT_FACING; vec2 SCREEN_UV;
vec3 ALBEDO; float ROUGHNESS; float METALLIC; vec3 EMISSION; float ALPHA; float ALPHA_SCISSOR;
vec3 NORMAL_MAP; float NORMAL_MAP_DEPTH; float AO;
float CLEARCOAT; float CLEARCOAT_ROUGHNESS; vec3 SHEEN_COLOR; float SHEEN_ROUGHNESS;
float ANISOTROPY; float ANISOTROPY_ROTATION; float TRANSMISSION; float IOR; float THICKNESS;
vec3 ATTENUATION_COLOR; float ATTENUATION_DISTANCE; float SUBSURFACE; vec3 SUBSURFACE_COLOR;
float SUBSURFACE_RADIUS; float WETNESS;
)";

inline constexpr const char* kSurfaceBuiltinsInit = R"(    ALBEDO = vec3(1.0); ROUGHNESS = 0.6; METALLIC = 0.0; EMISSION = vec3(0.0); ALPHA = 1.0;
    ALPHA_SCISSOR = 0.5; NORMAL_MAP = vec3(0.5, 0.5, 1.0); NORMAL_MAP_DEPTH = 1.0; AO = 1.0;
    CLEARCOAT = 0.0; CLEARCOAT_ROUGHNESS = 0.1; SHEEN_COLOR = vec3(0.0); SHEEN_ROUGHNESS = 0.5;
    ANISOTROPY = 0.0; ANISOTROPY_ROTATION = 0.0; TRANSMISSION = 0.0; IOR = 1.5; THICKNESS = 0.01;
    ATTENUATION_COLOR = vec3(1.0); ATTENUATION_DISTANCE = 1.0; SUBSURFACE = 0.0;
    SUBSURFACE_COLOR = vec3(1.0); SUBSURFACE_RADIUS = 0.5; WETNESS = 0.0;
)";

[[nodiscard]] std::string embedded_text(const std::uint8_t* data, std::size_t size);
[[nodiscard]] std::vector<GlslInclude> engine_includes();
[[nodiscard]] std::string wrap_body(const ShaderSource& source, const UniformLayout& layout, const std::string& body);
void collect_errors(const GlslCompile& c, std::vector<ShaderError>& errors, const char* stage);

} // namespace astral::detail
