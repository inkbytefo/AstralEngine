#include "paths/ray_traced_path.hpp"

#include "gpu_layouts.hpp"
#include "passes/frame_inputs.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace astral {

namespace {

float halton(std::uint32_t index, std::uint32_t base) {
    float f = 1.0f;
    float r = 0.0f;
    while (index > 0) {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(index % base);
        index /= base;
    }
    return r;
}

bool camera_changed(const Mat4& a, const Mat4& b, const Vec3& pos_a, const Vec3& pos_b) {
    if (length(pos_a - pos_b) > 1e-5f) return true;
    for (int i = 0; i < 4; ++i) {
        const Vec4& ca = a.cols[i];
        const Vec4& cb = b.cols[i];
        if (std::fabs(ca.x - cb.x) + std::fabs(ca.y - cb.y) + std::fabs(ca.z - cb.z) + std::fabs(ca.w - cb.w) > 1e-5f) {
            return true;
        }
    }
    return false;
}

GpuFrame build_rt_frame(const CameraData& camera, const RenderScene& scene, const RenderSettings& settings,
                        const DdgiFrameInfo& ddgi, const TemporalState& temporal, const FrameInstances& instances,
                        const FrameAddresses& addr, std::uint32_t pt_samples, gpu::Extent2D extent,
                        gpu::Format target_format, const Mat4& unjittered) {
    const std::uint32_t jitter_index = (temporal.frame_index % 16) + 1;
    const float jx = settings.path_tracer ? 0.0f : (halton(jitter_index, 2) - 0.5f);
    const float jy = settings.path_tracer ? 0.0f : (halton(jitter_index, 3) - 0.5f);
    const Mat4 jittered =
        settings.path_tracer
            ? unjittered
            : (translate({2.0f * jx / static_cast<float>(extent.width), 2.0f * jy / static_cast<float>(extent.height), 0}) *
               unjittered);
    const Vec3 sun = normalize(scene.sun.direction);
    const float sun_on = scene.sun.intensity;

    GpuFrame frame{};
    frame.view_projection = jittered;
    frame.inv_view_projection = inverse(jittered);
    frame.light_view_projection = Mat4::identity();
    frame.camera_position = {camera.position.x, camera.position.y, camera.position.z, 1};
    frame.atmosphere.aerial = settings.debug_view == DebugView::None ? aerial_parameters(scene.environment) : Vec4{};
    frame.sun_direction = {sun.x, sun.y, sun.z, 0};
    frame.sun_color = {scene.sun.color.x * sun_on, scene.sun.color.y * sun_on, scene.sun.color.z * sun_on, 0};
    frame.sky_color = {scene.environment.sky.x, scene.environment.sky.y, scene.environment.sky.z,
                       scene.environment.ambient};
    frame.ground_color = {scene.environment.ground.x, scene.environment.ground.y, scene.environment.ground.z,
                          scene.environment.exposure};
    frame.counts[0] = static_cast<std::uint32_t>(instances.lights.size());
    frame.counts[1] = static_cast<std::uint32_t>(instances.shapes.size());
    frame.counts[2] = no_shadow;
    const bool restir_active = settings.restir_di && (instances.lights.size() + instances.rects.size()) > 2;
    const bool restir_history_valid =
        restir_active && temporal.history_valid && temporal.prev_restir_active &&
        temporal.prev_point_lights == static_cast<std::uint32_t>(instances.lights.size()) &&
        temporal.prev_rect_lights == static_cast<std::uint32_t>(instances.rects.size());
    frame.counts[3] = (restir_active ? 1u : 0u) | (restir_history_valid ? 2u : 0u);
    frame.screen = {static_cast<float>(extent.width), static_cast<float>(extent.height),
                    1.0f / static_cast<float>(extent.width), 1.0f / static_cast<float>(extent.height)};
    frame.prev_view_projection = temporal.history_valid ? temporal.prev_view_projection : unjittered;
    frame.ddgi_origin = {ddgi.origin.x, ddgi.origin.y, ddgi.origin.z, ddgi.valid ? settings.gi.hysteresis : 0.0f};
    frame.ddgi_spacing = {ddgi.spacing.x, ddgi.spacing.y, ddgi.spacing.z, 0};
    frame.ddgi_counts[0] = ddgi.nx;
    frame.ddgi_counts[1] = ddgi.ny;
    frame.ddgi_counts[2] = ddgi.nz;
    frame.ddgi_counts[3] = ddgi.rays;
    frame.rt_counts[0] = static_cast<std::uint32_t>(instances.rects.size());
    frame.rt_counts[1] = temporal.frame_index;
    frame.rt_counts[2] = temporal.history_valid ? 1u : 0u;
    frame.rt_counts[3] = ddgi.valid ? 1u : 0u;
    frame.rt_params = {std::max(scene.sun.angular_radius, 0.0f), jx, jy, settings.max_ray_distance};
    frame.unjittered_view_projection = unjittered;
    frame.custom[0] = static_cast<std::uint32_t>(addr.params);
    frame.custom[1] = static_cast<std::uint32_t>(addr.params >> 32);
    std::memcpy(&frame.custom[2], &scene.time, sizeof(float));
    frame.custom[3] = is_srgb(target_format) ? 0u : 1u;
    const Vec3 prev_cam = temporal.history_valid ? temporal.prev_camera_position : camera.position;
    frame.prev_camera_position = {prev_cam.x, prev_cam.y, prev_cam.z, 1};
    frame.inv_prev_view_projection = inverse(frame.prev_view_projection);
    frame.scene = {addr.tlas, addr.instances, addr.materials, addr.lights, addr.rects, addr.shapes, addr.decals};
    frame.decal_count = static_cast<std::uint32_t>(instances.decals.size());
    const std::uint32_t bounces = std::clamp(settings.path_tracer_max_bounces, 1u, 16u);
    frame.pt_params = (pt_samples & 0xFFFFu) | ((bounces & 0xFFu) << 16u);
    return frame;
}

FrameAddresses collect_rt_addresses(gpu::Device& device, const FrameSlot& slot, const FrameInstances& instances) {
    return {
        .frame = device.buffer_address(slot.frame.buffer),
        .instances = device.buffer_address(slot.instances.buffer),
        .materials = device.buffer_address(slot.materials.buffer),
        .lights = device.buffer_address(slot.lights.buffer),
        .shapes = device.buffer_address(slot.shapes.buffer),
        .rects = device.buffer_address(slot.rects.buffer),
        .decals = device.buffer_address(slot.decals.buffer),
        .params = device.buffer_address(slot.params.buffer),
        .tlas = device.acceleration_structure_address(instances.tlas),
    };
}

ShadowFrame prepare_fog_shadow(RayTracedPathContext& ctx, const RenderScene& scene, GpuFrame& frame) {
    if (ctx.settings.path_tracer || ctx.settings.debug_view != DebugView::None) return {};
    frame.atmosphere.fog = fog_parameters(scene);
    if (frame.atmosphere.fog.x == 0 && scene.smoke.empty()) return {};
    const auto shadow = ctx.passes.shadow.prepare(scene, ctx.meshes);
    frame.light_view_projection = shadow.light_view_projection;
    return shadow;
}

void record_rt_graph(RayTracedPathContext& ctx, const FrameInputs& inputs, gpu::TextureHandle target,
                     const RtPipelines& rt_pipes, const ShadowFrame& fog_shadow) {
    RenderGraph& g = ctx.graph;
    g.begin_frame();
    const RGTexture output = g.import(target, "output");
    ctx.passes.sky.bake_environment(g, inputs);
    GBufferTextures gbuffer = ctx.passes.gbuffer.add(g, inputs, ctx.stats);
    ctx.passes.sdf.add(g, inputs, gbuffer);
    ctx.passes.decals.add(g, inputs, gbuffer);
    const DdgiOutput ddgi = ctx.passes.ddgi.add(g, inputs, rt_pipes.ddgi_trace);
    const RtLightingOutput rt = ctx.passes.rt_lighting.add(g, inputs, gbuffer, ddgi, rt_pipes);
    const DenoiseOutput denoised = ctx.passes.denoise.add(g, inputs, gbuffer, rt);
    const RGTexture hdr = ctx.passes.composite.add(g, inputs, gbuffer, denoised);
    ctx.passes.sky.add(g, inputs, hdr, gbuffer);
    const RGTexture atmospheric_hdr = inputs.settings.debug_view == DebugView::None ?
        ctx.passes.aerial.add(g, inputs, hdr, gbuffer) : hdr;
    const RGTexture taa_out = ctx.passes.taa.add(g, inputs, gbuffer, atmospheric_hdr);
    const RGTexture shadow = fog_shadow.bindless_index != 0xFFFFFFFFu ?
        ctx.passes.shadow.add(g, inputs, fog_shadow, ctx.stats) : RGTexture{};
    const RGTexture fog = ctx.passes.fog.integrate(g, inputs, shadow, ctx.stats, ddgi);
    const RGTexture fogged = ctx.passes.fog.composite(g, inputs, taa_out, gbuffer, fog);
    const RGTexture bloom = ctx.passes.bloom.add(g, inputs, fogged);

    const std::span<const MaterialHandle> post_chain = ctx.passes.post.collect(inputs.scene, ctx.materials);
    const RGTexture scene_color =
        post_chain.empty() ? output
                           : g.create("scene.ldr", {inputs.extent.width, inputs.extent.height, ctx.target_format});
    ctx.passes.tonemap.add(g, inputs, fogged, bloom, scene_color);
    ctx.passes.transparent.add(g, inputs, scene_color, gbuffer.depth, ctx.stats, fog);
    ctx.passes.particles.add(g, inputs, scene_color, gbuffer, fog, ddgi);
    ctx.passes.post.add(g, inputs, scene_color, output, post_chain);
    ctx.passes.debug_lines.add(g, inputs, output, gbuffer.depth);
    ctx.passes.ui.add(g, inputs, output);
}

void record_path_tracer_graph(RayTracedPathContext& ctx, const FrameInputs& inputs, gpu::TextureHandle target,
                              const RtPipelines& rt_pipes) {
    RenderGraph& g = ctx.graph;
    g.begin_frame();
    const RGTexture output = g.import(target, "output");
    ctx.passes.sky.bake_environment(g, inputs);
    const bool need_depth =
        inputs.scene.sky.valid() || aerial_parameters(inputs.scene.environment).x > 0.0f ||
        !inputs.instances.transparent_draws.empty() || !inputs.scene.debug_lines.empty() || !inputs.scene.particles.empty();
    GBufferTextures gbuffer{};
    if (need_depth) {
        gbuffer = ctx.passes.gbuffer.add(g, inputs, ctx.stats);
        ctx.passes.sdf.add(g, inputs, gbuffer);
    }
    const RGTexture hdr = ctx.passes.path_tracer.add(g, inputs, rt_pipes.path_tracer);
    if (need_depth) ctx.passes.sky.add(g, inputs, hdr, gbuffer);
    const RGTexture atmospheric_hdr = need_depth && inputs.settings.debug_view == DebugView::None ? ctx.passes.aerial.add(g, inputs, hdr, gbuffer) : hdr;
    const RGTexture bloom = ctx.passes.bloom.add(g, inputs, atmospheric_hdr);
    const std::span<const MaterialHandle> post_chain = ctx.passes.post.collect(inputs.scene, ctx.materials);
    const RGTexture scene_color =
        post_chain.empty() ? output
                           : g.create("scene.ldr", {inputs.extent.width, inputs.extent.height, ctx.target_format});
    ctx.passes.tonemap.add(g, inputs, atmospheric_hdr, bloom, scene_color);
    if (need_depth) {
        ctx.passes.transparent.add(g, inputs, scene_color, gbuffer.depth, ctx.stats);
        ctx.passes.particles.add(g, inputs, scene_color, gbuffer);
    }
    ctx.passes.post.add(g, inputs, scene_color, output, post_chain);
    if (need_depth) ctx.passes.debug_lines.add(g, inputs, output, gbuffer.depth);
    ctx.passes.ui.add(g, inputs, output);
}

void reset_path_tracer_history(RayTracedPathContext& ctx, const RenderScene& scene, const CameraData& camera,
                               const Mat4& unjittered) {
    const auto sky_key = ctx.passes.sky.environment_key(scene, ctx.settings, ctx.materials);
    const bool sky_changed = sky_key != ctx.temporal.sky_key;
    ctx.temporal.sky_key = sky_key;
    if (sky_changed || !ctx.temporal.history_valid ||
        camera_changed(unjittered, ctx.temporal.prev_view_projection, camera.position, ctx.temporal.prev_camera_position) ||
        ctx.temporal.prev_point_lights != static_cast<std::uint32_t>(ctx.frame_instances.lights.size()) ||
        ctx.temporal.prev_rect_lights != static_cast<std::uint32_t>(ctx.frame_instances.rects.size())) {
        ctx.passes.path_tracer.reset();
    }
}

} // namespace

void render_ray_traced_path(RayTracedPathContext& ctx, gpu::CommandList& cmd, gpu::TextureHandle target,
                            const CameraData& camera, const RenderScene& scene, gpu::Extent2D extent) {
    ctx.shaders.rebuild_rt_surfaces_if_dirty();
    const RtPipelines& rt_pipes = ctx.shaders.rt_pipelines();
    FrameSlot& slot = ctx.ring.acquire(ctx.device, cmd);

    bool resized = false;
    resized |= ctx.passes.rt_lighting.resize(extent.width, extent.height);
    resized |= ctx.passes.denoise.resize(extent.width, extent.height);
    resized |= ctx.passes.taa.resize(extent.width, extent.height);
    resized |= ctx.passes.path_tracer.resize(extent.width, extent.height);
    if (resized) {
        ctx.temporal.history_valid = false;
        ctx.passes.path_tracer.reset();
    }
    ctx.passes.ddgi.ensure(scene, ctx.meshes, ctx.settings.gi);

    prepare_instances(ctx.device, scene, camera.position, ctx.meshes, ctx.materials, slot, ctx.frame_instances, true);
    upload_scene_buffers(ctx.device, scene, ctx.textures, slot, ctx.frame_instances, true);
    if (!build_tlas(ctx.device, cmd, ctx.frame_instances)) return;

    const Mat4 unjittered = camera.projection * camera.view;
    reset_path_tracer_history(ctx, scene, camera, unjittered);

    FrameAddresses addresses = collect_rt_addresses(ctx.device, slot, ctx.frame_instances);
    GpuFrame frame =
        build_rt_frame(camera, scene, ctx.settings, ctx.passes.ddgi.info(), ctx.temporal, ctx.frame_instances,
                       addresses, ctx.passes.path_tracer.sample_count(), extent, ctx.target_format, unjittered);
    const ShadowFrame fog_shadow = prepare_fog_shadow(ctx, scene, frame);
    slot.frame.write(ctx.device, &frame, sizeof(frame));
    addresses.frame = ctx.device.buffer_address(slot.frame.buffer);

    const FrameInputs inputs{
        .camera = camera,
        .scene = scene,
        .settings = ctx.settings,
        .meshes = ctx.meshes,
        .materials = ctx.materials,
        .shaders = ctx.shaders,
        .textures = ctx.textures,
        .instances = ctx.frame_instances,
        .slot = slot,
        .addresses = addresses,
        .extent = extent,
        .target_format = ctx.target_format,
        .frame_index = ctx.temporal.frame_index,
        .view_projection = unjittered,
    };

    if (ctx.settings.path_tracer) {
        record_path_tracer_graph(ctx, inputs, target, rt_pipes);
    } else {
        record_rt_graph(ctx, inputs, target, rt_pipes, fog_shadow);
        ctx.passes.ddgi.mark_valid();
    }
    ctx.graph.execute(cmd);

    ctx.temporal.prev_view_projection = unjittered;
    ctx.temporal.prev_camera_position = camera.position;
    ctx.temporal.prev_point_lights = static_cast<std::uint32_t>(ctx.frame_instances.lights.size());
    ctx.temporal.prev_rect_lights = static_cast<std::uint32_t>(ctx.frame_instances.rects.size());
    ctx.temporal.prev_restir_active =
        ctx.settings.restir_di && (ctx.frame_instances.lights.size() + ctx.frame_instances.rects.size()) > 2;
    ctx.temporal.history_valid = true;
    ++ctx.temporal.frame_index;

    ctx.stats.ray_traced = true;
    ctx.stats.sdf_shapes = static_cast<std::uint32_t>(ctx.frame_instances.shapes.size());
    ctx.stats.point_lights = static_cast<std::uint32_t>(ctx.frame_instances.lights.size());
    ctx.stats.rect_lights = static_cast<std::uint32_t>(ctx.frame_instances.rects.size());
    ctx.stats.decals = static_cast<std::uint32_t>(ctx.frame_instances.decals.size());
    ctx.stats.path_tracer_samples = ctx.settings.path_tracer ? ctx.passes.path_tracer.sample_count() : 0u;
    ctx.stats.gi_probes = ctx.passes.ddgi.probe_count();
    ctx.stats.passes = ctx.graph.stats().passes_executed;
}

} // namespace astral
