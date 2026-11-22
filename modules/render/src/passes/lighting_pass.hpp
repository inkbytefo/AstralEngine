#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>

namespace astral {

/// Raster-path deferred PBR lighting pass (meshes + optional SDF + sun shadow map + point lights).
struct LightingPass {
    explicit LightingPass(gpu::Device& device);
    ~LightingPass();
    LightingPass(const LightingPass&) = delete;
    LightingPass& operator=(const LightingPass&) = delete;

    [[nodiscard]] RGTexture add(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                                RGTexture shadow_map) const;

private:
    gpu::Device& device_;
    gpu::PipelineHandle pipeline_{};
};

} // namespace astral
