#pragma once

#include "frame/frame_slot.hpp"
#include "frame/instances.hpp"
#include "passes/render_passes.hpp"
#include "stores/material_store.hpp"
#include "stores/mesh_store.hpp"
#include "stores/shader_store.hpp"
#include "stores/texture_store.hpp"

#include <astral/gpu/command_list.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>
#include <astral/render/renderer.hpp>

namespace astral {

struct RasterPathContext {
    gpu::Device& device;
    RenderGraph& graph;
    const MeshStore& meshes;
    MaterialStore& materials;
    ShaderStore& shaders;
    const TextureStore& textures;
    FrameInstances& frame_instances;
    FrameRing& ring;
    RenderPasses& passes;
    const RenderSettings& settings;
    gpu::Format target_format;
    RenderStats& stats;
};

/// Records a raster deferred frame (shadow -> gbuffer -> sdf -> lighting -> sky -> tonemap -> transparent -> post ->
/// debug -> ui).
void render_raster_path(RasterPathContext& ctx, gpu::CommandList& cmd, gpu::TextureHandle target,
                        const CameraData& camera, const RenderScene& scene, gpu::Extent2D extent);

} // namespace astral
