#include "stores/shader_store.hpp"

#include <shaders/ddgi_trace.comp.hpp>
#include <shaders/path_tracer.comp.hpp>
#include <shaders/restir_di_spatial.comp.hpp>
#include <shaders/restir_di_temporal.comp.hpp>
#include <shaders/rt_lighting.comp.hpp>

#include <span>
#include <string>
#include <vector>

namespace astral {

namespace {

RtPipelines make_default_rt_pipelines(gpu::Device& device) {
    return {
        .rt_lighting = device.create_compute_pipeline({.shader = {shaders::rt_lighting_comp}, .debug_name = "astral.rt_lighting"}),
        .ddgi_trace = device.create_compute_pipeline({.shader = {shaders::ddgi_trace_comp}, .debug_name = "astral.ddgi_trace"}),
        .restir_temporal = device.create_compute_pipeline({.shader = {shaders::restir_di_temporal_comp}, .debug_name = "astral.restir_di_temporal"}),
        .restir_spatial = device.create_compute_pipeline({.shader = {shaders::restir_di_spatial_comp}, .debug_name = "astral.restir_di_spatial"}),
        .path_tracer = device.create_compute_pipeline({.shader = {shaders::path_tracer_comp}, .debug_name = "astral.path_tracer"}),
    };
}

void destroy_pipelines(gpu::Device& device, RtPipelines& p) {
    device.destroy(p.rt_lighting);
    device.destroy(p.ddgi_trace);
    device.destroy(p.restir_temporal);
    device.destroy(p.restir_spatial);
    device.destroy(p.path_tracer);
    p = {};
}

} // namespace

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
    shader.alpha_test = build.alpha_test;
    shader.rt_chunk = build.rt_chunk;
    shader.samplers = build.samplers;
    shader.gbuffer = device_.create_graphics_pipeline({
        .vertex = {build.gbuffer_vertex},
        .fragment = {build.gbuffer_fragment},
        .color_formats = {gpu::Format::RGBA8Unorm, gpu::Format::RGBA16Float, gpu::Format::RGBA16Float, gpu::Format::RG16Float,
                          gpu::Format::RGBA16Float},
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
    if (rt_.valid()) return true;
    rt_ = make_default_rt_pipelines(device_);
    if (!rt_.valid()) {
        destroy_rt_pipelines();
        return false;
    }
    return true;
}

void ShaderStore::destroy_rt_pipelines() { destroy_pipelines(device_, rt_); }

/// Compiles the ray-traced compute shaders with every live custom surface() in; with none (or if the
/// compile fails) the precompiled ones are used and the error is kept for shader_error().
void ShaderStore::rebuild_rt_surfaces_if_dirty() {
    if (!rt_surfaces_dirty_) return;
    rt_surfaces_dirty_ = false;
    std::vector<RtSurface> surfaces;
    shaders_.for_each([&](ShaderHandle h, GpuShader& s) {
        if (!s.rt_chunk.empty()) surfaces.push_back({h.index, s.rt_chunk, s.alpha_test});
    });
    RtPipelines next{};
    rt_surface_error_.clear();
    if (!surfaces.empty()) {
        const RtVariants v = compile_rt_variants(surfaces);
        if (v.ok()) {
            next.rt_lighting = device_.create_compute_pipeline({.shader = {v.lighting}, .debug_name = "astral.rt_lighting.custom"});
            next.ddgi_trace = device_.create_compute_pipeline({.shader = {v.ddgi_trace}, .debug_name = "astral.ddgi_trace.custom"});
            next.restir_temporal = device_.create_compute_pipeline({.shader = {v.restir_temporal}, .debug_name = "astral.restir_di_temporal.custom"});
            next.restir_spatial = device_.create_compute_pipeline({.shader = {v.restir_spatial}, .debug_name = "astral.restir_di_spatial.custom"});
            next.path_tracer = device_.create_compute_pipeline({.shader = {v.path_tracer}, .debug_name = "astral.path_tracer.custom"});
        } else {
            for (const ShaderError& e : v.errors) rt_surface_error_ += "line " + std::to_string(e.line) + ": " + e.message + "\n";
        }
    }
    const bool custom = next.valid();
    if (!custom) {
        destroy_pipelines(device_, next);
        if (!rt_custom_active_ && rt_.valid()) return;
        next = make_default_rt_pipelines(device_);
    }
    destroy_rt_pipelines();
    rt_ = next;
    rt_custom_active_ = custom;
}

} // namespace astral
