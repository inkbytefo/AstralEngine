#pragma once
// shader_type surface (ADR-0022): generates the G-buffer and shadow stages of a .ashader around the
// engine's templates and compiles them to SPIR-V. Built-ins:
//   vertex():  VERTEX, NORMAL (object space, writable), UV, WORLD_POSITION, MODEL_MATRIX, TIME, uniforms, varyings
//   surface(): UV, NORMAL (world), WORLD_POSITION, VIEW_DIR, FRONT_FACING, TIME, SCREEN_UV; write ALBEDO,
//              ROUGHNESS, METALLIC, EMISSION, ALPHA, ALPHA_SCISSOR, NORMAL_MAP, NORMAL_MAP_DEPTH, AO,
//              CLEARCOAT, CLEARCOAT_ROUGHNESS, SHEEN_COLOR, SHEEN_ROUGHNESS, ANISOTROPY, ANISOTROPY_ROTATION,
//              TRANSMISSION, IOR, THICKNESS, ATTENUATION_COLOR, ATTENUATION_DISTANCE, SUBSURFACE, SUBSURFACE_COLOR,
//              SUBSURFACE_RADIUS, WETNESS
// Uniforms live in the material's parameter block (layout_uniforms); textures are bindless indices.

#include <astral/shader/ashader.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace astral {

struct SurfaceShaderBuild {
    std::vector<std::uint32_t> gbuffer_vertex, gbuffer_fragment;
    std::vector<std::uint32_t> shadow_vertex;
    std::vector<std::uint32_t> shadow_fragment; ///< Only with render_mode alpha_test.
    bool cull_disabled = false;
    bool cull_front = false;
    bool casts_shadows = true;
    UniformLayout layout;
    /// The sampler2D uniforms: where their bindless index goes in the parameter block.
    struct Sampler {
        std::uint32_t offset = 0;
        bool normal = false; ///< hint_normal: unset reads as a flat normal instead of white.
    };
    std::vector<Sampler> samplers;
    /// Ray-traced variant of surface(): GLSL for the RT compute shaders, with "@ID@" standing for the
    /// shader's slot (see compose_rt_surfaces). Empty if there is no surface().
    std::string rt_chunk;
    /// render_mode blend_mix / blend_add: drawn in the forward pass after opaque geometry, sorted back to
    /// front; there are no G-buffer / shadow stages and no ray-traced variant.
    std::vector<std::uint32_t> forward_vertex, forward_fragment;
    bool blend = false;
    bool additive = false;
    bool alpha_test = false;
    std::vector<ShaderError> errors; ///< Lines of the .ashader file.
    [[nodiscard]] bool ok() const noexcept { return errors.empty() && (!gbuffer_fragment.empty() || !forward_fragment.empty()); }
};

/// shader_type post / sky (ADR-0022): a full-screen fragment shader.
///   post():  COLOR (rgb, the displayed colour: write it), SCREEN_UV, SCREEN_TEXTURE (sampler2D), TIME
///   sky():   EYEDIR, SUN_DIR, SUN_COLOR, TIME; write COLOR (linear HDR, before tonemapping)
struct ScreenShaderBuild {
    ShaderType type = ShaderType::Post;
    std::vector<std::uint32_t> vertex, fragment, compute, sky_capture;
    bool additive = true;
    bool sky_uses_time = false;
    UniformLayout layout;
    std::vector<SurfaceShaderBuild::Sampler> samplers;
    std::vector<ShaderError> errors;
    [[nodiscard]] bool ok() const noexcept { return errors.empty() && (!fragment.empty() || !compute.empty()); }
};
[[nodiscard]] ScreenShaderBuild compile_screen_shader(const ShaderSource& source, std::string_view name);

/// Generates and compiles every stage. Thread-safe (compile in a job for hot reload).
[[nodiscard]] SurfaceShaderBuild compile_surface_shader(const ShaderSource& source, std::string_view name);

/// One shader's part of the ray-traced include: `slot` is its ShaderHandle index.
struct RtSurface {
    std::uint32_t slot = 0;
    std::string chunk; ///< SurfaceShaderBuild::rt_chunk
    bool alpha_test = false;
};
/// The text of <astral/rt_custom.glsl>: every shader's surface() as a function of the ray-traced
/// hit, and astral_custom_surface() choosing by slot. Differences from the raster stage: no
/// derivatives (dFdx/dFdy read 0), texture() samples a fixed coarse mip, varyings are 0 (vertex() does
/// not run), NORMAL_MAP and ALPHA are ignored.
[[nodiscard]] std::string compose_rt_surfaces(std::span<const RtSurface> surfaces);

/// The ray-traced compute shaders with the custom surfaces compiled in (empty spirv + errors on failure).
struct RtVariants {
    std::vector<std::uint32_t> lighting, ddgi_trace, restir_temporal, restir_spatial, path_tracer;
    std::vector<ShaderError> errors;
    std::string log;
    [[nodiscard]] bool ok() const noexcept {
        return errors.empty() && !lighting.empty() && !ddgi_trace.empty() && !restir_temporal.empty() &&
               !restir_spatial.empty() && !path_tracer.empty();
    }
};
[[nodiscard]] RtVariants compile_rt_variants(std::span<const RtSurface> surfaces);

} // namespace astral
