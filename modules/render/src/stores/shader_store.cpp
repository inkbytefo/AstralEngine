#include "stores/shader_store.hpp"

#include <shaders/ddgi_trace.comp.hpp>
#include <shaders/rt_lighting.comp.hpp>

#include <span>
#include <string>
#include <vector>

namespace astral {

ShaderStore::~ShaderStore() {
    shaders_.for_each([&](ShaderHandle, GpuShader& shader) {
        device_.destroy(shader.forward);
        device_.destroy(shader.gbuffer);
        device_.destroy(shader.shadow);
    });
    destroy_rt_pipelines();
}

ShaderHandle ShaderStore::create(const SurfaceShaderBuild& build, std::string_view debug_name) {
    if (!build.ok() || (!build.blend && build.shadow_vertex.empty())) return {};
    const std::string name = debug_name.empty() ? std::string("astral.ashader") : std::string(debug_name);
    const gpu::CullMode cull =
        build.cull_disabled ? gpu::CullMode::None : (build.cull_front ? gpu::CullMode::Front : gpu::CullMode::Back);
    if (build.blend) {
        GpuShader blended;
        blended.blend = true;
        blended.samplers = build.samplers;
        blended.forward = device_.create_graphics_pipeline({
            .vertex = {build.forward_vertex},
            .fragment = {build.forward_fragment},
            .color_formats = {target_format_},
            .depth = {.format = gpu::Format::D32Float, .test = true, .write = false, .compare = gpu::CompareOp::LessEqual},
            .cull = cull,
            .alpha_blend = !build.additive,
            .additive_blend = build.additive,
            .debug_name = name + ".forward",
        });
        if (!blended.forward.valid()) return {};
        return shaders_.create(blended);
    }
    GpuShader shader;
    shader.casts_shadows = build.casts_shadows;
    shader.rt_chunk = build.rt_chunk;
    shader.samplers = build.samplers;
    shader.gbuffer = device_.create_graphics_pipeline({
        .vertex = {build.gbuffer_vertex},
        .fragment = {build.gbuffer_fragment},
        .color_formats = {gpu::Format::RGBA8Unorm, gpu::Format::RGBA16Float, gpu::Format::RGBA16Float, gpu::Format::RG16Float},
        .depth = {.format = gpu::Format::D32Float},
        .cull = cull,
        .debug_name = name + ".gbuffer",
    });
    shader.shadow = device_.create_graphics_pipeline({
        .vertex = {build.shadow_vertex},
        .fragment = {build.shadow_fragment},
        .color_formats = {},
        .depth = {.format = gpu::Format::D32Float},
        .cull = gpu::CullMode::None,
        .debug_name = name + ".shadow",
    });
    if (!shader.gbuffer.valid() || !shader.shadow.valid()) {
        device_.destroy(shader.gbuffer);
        device_.destroy(shader.shadow);
        return {};
    }
    rt_surfaces_dirty_ = true;
    return shaders_.create(shader);
}

ShaderHandle ShaderStore::create(const ScreenShaderBuild& build, std::string_view debug_name) {
    if (!build.ok()) return {};
    const std::string name = debug_name.empty() ? std::string("astral.screen") : std::string(debug_name);
    const bool sky = build.type == ShaderType::Sky;
    GpuShader shader;
    shader.kind = sky ? GpuShader::Kind::Sky : GpuShader::Kind::Post;
    shader.samplers = build.samplers;
    shader.forward = device_.create_graphics_pipeline({
        .vertex = {build.vertex},
        .fragment = {build.fragment},
        .color_formats = {sky ? gpu::Format::RGBA16Float : target_format_},
        // The sky only fills what is still at the far plane; post effects cover everything.
        .depth = sky ? gpu::DepthState{.format = gpu::Format::D32Float, .test = true, .write = false, .compare = gpu::CompareOp::LessEqual}
                     : gpu::DepthState{.format = gpu::Format::Undefined, .test = false, .write = false},
        .debug_name = name + (sky ? ".sky" : ".post"),
    });
    if (!shader.forward.valid()) return {};
    return shaders_.create(shader);
}

void ShaderStore::destroy(ShaderHandle handle) {
    if (GpuShader* s = shaders_.get(handle)) {
        device_.destroy(s->forward);
        device_.destroy(s->gbuffer); // deferred by the device until the GPU is done with it
        device_.destroy(s->shadow);
        shaders_.destroy(handle);
        rt_surfaces_dirty_ = true;
    }
}

const GpuShader* ShaderStore::find_by_key(std::uint32_t key) noexcept {
    if (key == 0) return nullptr;
    const GpuShader* found = nullptr;
    shaders_.for_each([&](ShaderHandle sh, GpuShader& s) {
        if (sh.index + 1 == key) found = &s;
    });
    return found;
}

bool ShaderStore::ensure_rt_pipelines() {
    if (rt_.rt_lighting.valid() && rt_.ddgi_trace.valid()) return true;
    rt_.rt_lighting =
        device_.create_compute_pipeline({.shader = {shaders::rt_lighting_comp}, .debug_name = "astral.rt_lighting"});
    rt_.ddgi_trace =
        device_.create_compute_pipeline({.shader = {shaders::ddgi_trace_comp}, .debug_name = "astral.ddgi_trace"});
    if (!rt_.rt_lighting.valid() || !rt_.ddgi_trace.valid()) {
        destroy_rt_pipelines(); // fall back to the raster path
        return false;
    }
    return true;
}

void ShaderStore::destroy_rt_pipelines() {
    device_.destroy(rt_.rt_lighting);
    device_.destroy(rt_.ddgi_trace);
    rt_.rt_lighting = {};
    rt_.ddgi_trace = {};
}

/// Compiles the ray-traced compute shaders with every live custom surface() in; with none (or if the
/// compile fails) the precompiled ones are used and the error is kept for shader_error().
void ShaderStore::rebuild_rt_surfaces_if_dirty() {
    if (!rt_surfaces_dirty_) return;
    rt_surfaces_dirty_ = false;
    std::vector<RtSurface> surfaces;
    shaders_.for_each([&](ShaderHandle h, GpuShader& s) {
        if (!s.rt_chunk.empty()) surfaces.push_back({h.index, s.rt_chunk});
    });
    gpu::PipelineHandle lit, trace;
    rt_surface_error_.clear();
    if (!surfaces.empty()) {
        const RtVariants variants = compile_rt_variants(surfaces);
        if (variants.ok()) {
            lit = device_.create_compute_pipeline({.shader = {variants.lighting}, .debug_name = "astral.rt_lighting.custom"});
            trace = device_.create_compute_pipeline({.shader = {variants.ddgi_trace}, .debug_name = "astral.ddgi_trace.custom"});
        } else {
            for (const ShaderError& e : variants.errors) {
                rt_surface_error_ += "line " + std::to_string(e.line) + ": " + e.message + "\n";
            }
        }
    }
    const bool custom = lit.valid() && trace.valid();
    if (!custom) {
        device_.destroy(lit);
        device_.destroy(trace);
        if (!rt_custom_active_ && rt_.rt_lighting.valid()) return; // already the precompiled ones
        lit = device_.create_compute_pipeline({.shader = {shaders::rt_lighting_comp}, .debug_name = "astral.rt_lighting"});
        trace = device_.create_compute_pipeline({.shader = {shaders::ddgi_trace_comp}, .debug_name = "astral.ddgi_trace"});
    }
    device_.destroy(rt_.rt_lighting);
    device_.destroy(rt_.ddgi_trace);
    rt_.rt_lighting = lit;
    rt_.ddgi_trace = trace;
    rt_custom_active_ = custom;
}

} // namespace astral
