#pragma once
// shader_type surface (ADR-0022): generates the G-buffer and shadow stages of a .ashader around the
// engine's templates and compiles them to SPIR-V. Built-ins:
//   vertex():  VERTEX, NORMAL (object space, writable), UV, WORLD_POSITION, MODEL_MATRIX, TIME, uniforms, varyings
//   surface(): UV, NORMAL (world), WORLD_POSITION, VIEW_DIR, FRONT_FACING, TIME, SCREEN_UV; write ALBEDO,
//              ROUGHNESS, METALLIC, EMISSION, ALPHA, ALPHA_SCISSOR, NORMAL_MAP, NORMAL_MAP_DEPTH, AO
// Uniforms live in the material's parameter block (layout_uniforms); textures are bindless indices.

#include <astral/shader/ashader.hpp>

#include <cstdint>
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
    std::vector<ShaderError> errors; ///< Lines of the .ashader file.
    [[nodiscard]] bool ok() const noexcept { return errors.empty() && !gbuffer_fragment.empty(); }
};

/// Generates and compiles every stage. Thread-safe (compile in a job for hot reload).
[[nodiscard]] SurfaceShaderBuild compile_surface_shader(const ShaderSource& source, std::string_view name);

} // namespace astral
