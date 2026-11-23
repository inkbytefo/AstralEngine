#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>

namespace astral {

/// Deferred box-projected decal pass (M13 / ADR-0033): skips completely when `scene.decals` is empty.
struct DecalPass {
    explicit DecalPass(gpu::Device& device);
    ~DecalPass();
    DecalPass(const DecalPass&) = delete;
    DecalPass& operator=(const DecalPass&) = delete;

    void add(RenderGraph& graph, const FrameInputs& inputs, GBufferTextures& gbuffer) const;

private:
    gpu::Device& device_;
    gpu::PipelineHandle pipeline_{};
};

} // namespace astral
