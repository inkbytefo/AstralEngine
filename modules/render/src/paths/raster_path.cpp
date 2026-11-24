#include "paths/raster_path.hpp"

#include "gpu_layouts.hpp"
#include "passes/frame_inputs.hpp"

#include <cstring>

namespace astral {

namespace {

GpuFrame build_raster_frame(const CameraData& camera, const RenderScene& scene, const ShadowFrame& shadow,
                            const FrameInstances& instances, const FrameAddresses& addr, gpu::Extent2D extent,
                            gpu::Format target_format, const Mat4& view_projection) {
    const Vec3 sun = normalize(scene.sun.direction);
    GpuFrame frame{};
    frame.view_projection = view_projection;
    frame.inv_view_projection = inverse(view_projection);
    frame.light_view_projection = shadow.light_view_projection;
    frame.camera_position = {camera.position.x, camera.position.y, camera.position.z, 1};
    frame.fog = fog_parameters(scene.environment);
    frame.aerial = aerial_parameters(scene.environment);
    frame.sun_direction = {sun.x, sun.y, sun.z, shadow.texel_world * 1.5f};
    frame.sun_color = {scene.sun.color.x * scene.sun.intensity, scene.sun.color.y * scene.sun.intensity,
                       scene.sun.color.z * scene.sun.intensity, 0};
    frame.sky_color = {scene.environment.sky.x, scene.environment.sky.y, scene.environment.sky.z,
                       scene.environment.ambient};
    frame.ground_color = {scene.environment.ground.x, scene.environment.ground.y, scene.environment.ground.z,
                          scene.environment.exposure};
    frame.counts[0] = static_cast<std::uint32_t>(instances.lights.size());
    frame.counts[1] = static_cast<std::uint32_t>(instances.shapes.size());
    frame.counts[2] = shadow.bindless_index;
    frame.screen = {static_cast<float>(extent.width), static_cast<float>(extent.height),
                    1.0f / static_cast<float>(extent.width), 1.0f / static_cast<float>(extent.height)};
    frame.prev_view_projection = Mat4::identity();
    frame.unjittered_view_projection = view_projection;
    frame.custom[0] = static_cast<std::uint32_t>(addr.params);
    frame.custom[1] = static_cast<std::uint32_t>(addr.params >> 32);
    std::memcpy(&frame.custom[2], &scene.time, sizeof(float));
    frame.custom[3] = is_srgb(target_format) ? 0u : 1u;
    frame.prev_camera_position = {camera.position.x, camera.position.y, camera.position.z, 1};
    frame.inv_prev_view_projection = Mat4::identity();
    frame.scene = {0, addr.instances, addr.materials, addr.lights, 0, addr.shapes, addr.decals};
    frame.decal_count = static_cast<std::uint32_t>(instances.decals.size());
    return frame;
}

FrameAddresses collect_addresses(gpu::Device& device, const FrameSlot& slot) {
    return {
        .frame = device.buffer_address(slot.frame.buffer),
        .instances = device.buffer_address(slot.instances.buffer),
        .materials = device.buffer_address(slot.materials.buffer),
        .lights = device.buffer_address(slot.lights.buffer),
        .shapes = device.buffer_address(slot.shapes.buffer),
        .rects = 0,
        .decals = device.buffer_address(slot.decals.buffer),
        .params = device.buffer_address(slot.params.buffer),
        .tlas = 0,
    };
}

} // namespace

void render_raster_path(RasterPathContext& ctx, gpu::CommandList& cmd, gpu::TextureHandle target,
                        const CameraData& camera, const RenderScene& scene, gpu::Extent2D extent) {
    FrameSlot& slot = ctx.ring.acquire(ctx.device, cmd);

    prepare_instances(ctx.device, scene, camera.position, ctx.meshes, ctx.materials, slot, ctx.frame_instances, false);
    upload_scene_buffers(ctx.device, scene, ctx.textures, slot, ctx.frame_instances, false);

    const ShadowFrame shadow = ctx.passes.shadow.prepare(scene, ctx.meshes);
    const Mat4 view_projection = camera.projection * camera.view;
    FrameAddresses addresses = collect_addresses(ctx.device, slot);
    const GpuFrame frame = build_raster_frame(camera, scene, shadow, ctx.frame_instances, addresses, extent,
                                              ctx.target_format, view_projection);
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
        .frame_index = 0,
        .view_projection = view_projection,
    };

    RenderGraph& g = ctx.graph;
    g.begin_frame();
    const RGTexture output = g.import(target, "output");
    const RGTexture shadow_map = ctx.passes.shadow.add(g, inputs, shadow, ctx.stats);
    GBufferTextures gbuffer = ctx.passes.gbuffer.add(g, inputs, ctx.stats);
    ctx.passes.sdf.add(g, inputs, gbuffer);
    ctx.passes.decals.add(g, inputs, gbuffer);
    const RGTexture hdr = ctx.passes.lighting.add(g, inputs, gbuffer, shadow_map);
    ctx.passes.sky.add(g, inputs, hdr, gbuffer);
    const RGTexture atmospheric_hdr = ctx.passes.aerial.add(g, inputs, hdr, gbuffer);

    const RGTexture fog = ctx.passes.fog.integrate(g, inputs, shadow_map);
    const RGTexture fogged = ctx.passes.fog.composite(g, inputs, atmospheric_hdr, gbuffer, fog);

    const std::span<const MaterialHandle> post_chain = ctx.passes.post.collect(scene, ctx.materials);
    const RGTexture scene_color =
        post_chain.empty() ? output : g.create("scene.ldr", {extent.width, extent.height, ctx.target_format});
    ctx.passes.tonemap.add(g, inputs, fogged, {}, scene_color);
    ctx.passes.transparent.add(g, inputs, scene_color, gbuffer.depth, ctx.stats, fog);
    ctx.passes.post.add(g, inputs, scene_color, output, post_chain);
    ctx.passes.debug_lines.add(g, inputs, output, gbuffer.depth);
    ctx.passes.ui.add(g, inputs, output);
    g.execute(cmd);

    ctx.stats.sdf_shapes = static_cast<std::uint32_t>(ctx.frame_instances.shapes.size());
    ctx.stats.point_lights = static_cast<std::uint32_t>(ctx.frame_instances.lights.size());
    ctx.stats.decals = static_cast<std::uint32_t>(ctx.frame_instances.decals.size());
    ctx.stats.passes = g.stats().passes_executed;
}

} // namespace astral
