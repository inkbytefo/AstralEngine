#include "paths/ray_traced_path.hpp"

#include "gpu_layouts.hpp"
#include "passes/frame_inputs.hpp"

#include <algorithm>
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

GpuFrame build_rt_frame(const CameraData& camera, const RenderScene& scene, const RenderSettings& settings,
                        const DdgiFrameInfo& ddgi, const TemporalState& temporal, const FrameInstances& instances,
                        gpu::Extent2D extent, gpu::Format target_format, std::uint64_t params_address,
                        const Mat4& unjittered) {
    const std::uint32_t jitter_index = (temporal.frame_index % 16) + 1;
    const float jx = halton(jitter_index, 2) - 0.5f;
    const float jy = halton(jitter_index, 3) - 0.5f;
    const Mat4 jittered =
        translate({2.0f * jx / static_cast<float>(extent.width), 2.0f * jy / static_cast<float>(extent.height), 0}) *
        unjittered;
    const Vec3 sun = normalize(scene.sun.direction);
    const float sun_on = scene.sun.intensity;

    GpuFrame frame{};
    frame.view_projection = jittered;
    frame.inv_view_projection = inverse(jittered);
    frame.light_view_projection = Mat4::identity();
    frame.camera_position = {camera.position.x, camera.position.y, camera.position.z, 1};
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
    frame.custom[0] = static_cast<std::uint32_t>(params_address);
    frame.custom[1] = static_cast<std::uint32_t>(params_address >> 32);
    std::memcpy(&frame.custom[2], &scene.time, sizeof(float));
    frame.custom[3] = is_srgb(target_format) ? 0u : 1u;
    const Vec3 prev_cam = temporal.history_valid ? temporal.prev_camera_position : camera.position;
    frame.prev_camera_position = {prev_cam.x, prev_cam.y, prev_cam.z, 1};
    frame.inv_prev_view_projection = inverse(frame.prev_view_projection);
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
        .params = device.buffer_address(slot.params.buffer),
        .tlas = device.acceleration_structure_address(instances.tlas),
    };
}

void record_rt_graph(RayTracedPathContext& ctx, const FrameInputs& inputs, gpu::TextureHandle target,
                     const RtPipelines& rt_pipes) {
    RenderGraph& g = ctx.graph;
    g.begin_frame();
    const RGTexture output = g.import(target, "output");
    GBufferTextures gbuffer = ctx.passes.gbuffer.add(g, inputs, ctx.stats);
    ctx.passes.sdf.add(g, inputs, gbuffer);
    const DdgiOutput ddgi = ctx.passes.ddgi.add(g, inputs, rt_pipes.ddgi_trace);
    const RtLightingOutput rt = ctx.passes.rt_lighting.add(g, inputs, gbuffer, ddgi, rt_pipes);
    const DenoiseOutput denoised = ctx.passes.denoise.add(g, inputs, gbuffer, rt);
    const RGTexture hdr = ctx.passes.composite.add(g, inputs, gbuffer, denoised);
    ctx.passes.sky.add(g, inputs, hdr, gbuffer);
    const RGTexture taa_out = ctx.passes.taa.add(g, inputs, gbuffer, hdr);
    const RGTexture bloom = ctx.passes.bloom.add(g, inputs, taa_out);

    const std::span<const MaterialHandle> post_chain = ctx.passes.post.collect(inputs.scene, ctx.materials);
    const RGTexture scene_color =
        post_chain.empty() ? output
                           : g.create("scene.ldr", {inputs.extent.width, inputs.extent.height, ctx.target_format});
    ctx.passes.tonemap.add(g, inputs, taa_out, bloom, scene_color);
    ctx.passes.transparent.add(g, inputs, scene_color, gbuffer.depth, ctx.stats);
    ctx.passes.post.add(g, inputs, scene_color, output, post_chain);
    ctx.passes.debug_lines.add(g, inputs, output, gbuffer.depth);
    ctx.passes.ui.add(g, inputs, output);
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
    if (resized) ctx.temporal.history_valid = false;
    ctx.passes.ddgi.ensure(scene, ctx.meshes, ctx.settings.gi);

    prepare_instances(ctx.device, scene, camera.position, ctx.meshes, ctx.materials, slot, ctx.frame_instances, true);
    upload_scene_buffers(ctx.device, scene, slot, ctx.frame_instances, true);
    if (!build_tlas(ctx.device, cmd, ctx.frame_instances)) return;

    const Mat4 unjittered = camera.projection * camera.view;
    const std::uint64_t params_address = ctx.device.buffer_address(slot.params.buffer);
    const GpuFrame frame = build_rt_frame(camera, scene, ctx.settings, ctx.passes.ddgi.info(), ctx.temporal,
                                          ctx.frame_instances, extent, ctx.target_format, params_address, unjittered);
    slot.frame.write(ctx.device, &frame, sizeof(frame));

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
        .addresses = collect_rt_addresses(ctx.device, slot, ctx.frame_instances),
        .extent = extent,
        .target_format = ctx.target_format,
        .frame_index = ctx.temporal.frame_index,
        .view_projection = unjittered,
    };

    record_rt_graph(ctx, inputs, target, rt_pipes);
    ctx.graph.execute(cmd);

    ctx.temporal.prev_view_projection = unjittered;
    ctx.temporal.prev_camera_position = camera.position;
    ctx.temporal.prev_point_lights = static_cast<std::uint32_t>(ctx.frame_instances.lights.size());
    ctx.temporal.prev_rect_lights = static_cast<std::uint32_t>(ctx.frame_instances.rects.size());
    ctx.temporal.prev_restir_active =
        ctx.settings.restir_di && (ctx.frame_instances.lights.size() + ctx.frame_instances.rects.size()) > 2;
    ctx.temporal.history_valid = true;
    ++ctx.temporal.frame_index;
    ctx.passes.ddgi.mark_valid();
    ctx.stats.ray_traced = true;
    ctx.stats.sdf_shapes = static_cast<std::uint32_t>(ctx.frame_instances.shapes.size());
    ctx.stats.point_lights = static_cast<std::uint32_t>(ctx.frame_instances.lights.size());
    ctx.stats.rect_lights = static_cast<std::uint32_t>(ctx.frame_instances.rects.size());
    ctx.stats.gi_probes = ctx.passes.ddgi.probe_count();
    ctx.stats.passes = ctx.graph.stats().passes_executed;
}

} // namespace astral
