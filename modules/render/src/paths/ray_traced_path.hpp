#pragma once

#include "frame/frame_slot.hpp"
#include "frame/instances.hpp"
#include "passes/render_passes.hpp"
#include "stores/material_store.hpp"
#include "stores/mesh_store.hpp"
#include "stores/shader_store.hpp"
#include "stores/texture_store.hpp"

#include <astral/core/math.hpp>
#include <astral/gpu/command_list.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>
#include <astral/render/renderer.hpp>

#include <cstdint>

namespace astral {

struct TemporalState {
    std::uint32_t frame_index = 0;
    bool history_valid = false;
    Mat4 prev_view_projection = Mat4::identity();
};

struct RayTracedPathContext {
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
    TemporalState& temporal;
    RenderStats& stats;
};

/// Records a hybrid ray-traced frame (ADR-0011): gbuffer -> sdf -> ddgi -> rt.lighting -> denoise -> composite ->
/// sky -> taa -> bloom -> tonemap -> transparent -> post -> debug -> ui.
void render_ray_traced_path(RayTracedPathContext& ctx, gpu::CommandList& cmd, gpu::TextureHandle target,
                            const CameraData& camera, const RenderScene& scene, gpu::Extent2D extent);

} // namespace astral
