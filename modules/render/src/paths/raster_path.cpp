#include "paths/raster_path.hpp"

#include "gpu_layouts.hpp"
#include "passes/frame_inputs.hpp"

#include <cstring>

namespace astral {

namespace {

GpuFrame build_raster_frame(const CameraData& camera, const RenderScene& scene, const ShadowFrame& shadow,
                            const FrameInstances& instances, gpu::Extent2D extent, gpu::Format target_format,
                            std::uint64_t params_address, const Mat4& view_projection) {
    const Vec3 sun = normalize(scene.sun.direction);
    GpuFrame frame{view_projection,
                   inverse(view_projection),
                   shadow.light_view_projection,
                   {camera.position.x, camera.position.y, camera.position.z, 1},
                   {sun.x, sun.y, sun.z, shadow.texel_world * 1.5f},
                   {scene.sun.color.x * scene.sun.intensity, scene.sun.color.y * scene.sun.intensity,
                    scene.sun.color.z * scene.sun.intensity, 0},
                   {scene.environment.sky.x, scene.environment.sky.y, scene.environment.sky.z,
                    scene.environment.ambient},
                   {scene.environment.ground.x, scene.environment.ground.y, scene.environment.ground.z,
                    scene.environment.exposure},
                   {static_cast<std::uint32_t>(instances.lights.size()),
                    static_cast<std::uint32_t>(instances.shapes.size()), shadow.bindless_index, 0},
                   {static_cast<float>(extent.width), static_cast<float>(extent.height),
                    1.0f / static_cast<float>(extent.width), 1.0f / static_cast<float>(extent.height)},
                   Mat4::identity(),
                   {},
                   {},
                   {},
                   {},
                   {},
                   view_projection,
                   {},
                   {camera.position.x, camera.position.y, camera.position.z, 1},
                   Mat4::identity()};
    frame.custom[0] = static_cast<std::uint32_t>(params_address);
    frame.custom[1] = static_cast<std::uint32_t>(params_address >> 32);
    std::memcpy(&frame.custom[2], &scene.time, sizeof(float));
    frame.custom[3] = is_srgb(target_format) ? 0u : 1u; // UNORM targets need shaders to encode
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
        .params = device.buffer_address(slot.params.buffer),
        .tlas = 0,
    };
}

} // namespace

void render_raster_path(RasterPathContext& ctx, gpu::CommandList& cmd, gpu::TextureHandle target,
                        const CameraData& camera, const RenderScene& scene, gpu::Extent2D extent) {
    FrameSlot& slot = ctx.ring.acquire(ctx.device, cmd);

    prepare_instances(ctx.device, scene, camera.position, ctx.meshes, ctx.materials, slot, ctx.frame_instances, false);
    upload_scene_buffers(ctx.device, scene, slot, ctx.frame_instances, false);

    const ShadowFrame shadow = ctx.passes.shadow.prepare(scene, ctx.meshes);
    const Mat4 view_projection = camera.projection * camera.view;
    const std::uint64_t params_address = ctx.device.buffer_address(slot.params.buffer);
    const GpuFrame frame = build_raster_frame(camera, scene, shadow, ctx.frame_instances, extent, ctx.target_format,
                                              params_address, view_projection);
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
        .addresses = collect_addresses(ctx.device, slot),
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
    const RGTexture hdr = ctx.passes.lighting.add(g, inputs, gbuffer, shadow_map);
    ctx.passes.sky.add(g, inputs, hdr, gbuffer);

    const std::span<const MaterialHandle> post_chain = ctx.passes.post.collect(scene, ctx.materials);
    const RGTexture scene_color =
        post_chain.empty() ? output : g.create("scene.ldr", {extent.width, extent.height, ctx.target_format});
    ctx.passes.tonemap.add(g, inputs, hdr, {}, scene_color);
    ctx.passes.transparent.add(g, inputs, scene_color, gbuffer.depth, ctx.stats);
    ctx.passes.post.add(g, inputs, scene_color, output, post_chain);
    ctx.passes.debug_lines.add(g, inputs, output, gbuffer.depth);
    ctx.passes.ui.add(g, inputs, output);
    g.execute(cmd);

    ctx.stats.sdf_shapes = static_cast<std::uint32_t>(ctx.frame_instances.shapes.size());
    ctx.stats.point_lights = static_cast<std::uint32_t>(ctx.frame_instances.lights.size());
    ctx.stats.passes = g.stats().passes_executed;
}

} // namespace astral
