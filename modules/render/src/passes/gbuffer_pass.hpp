#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>
#include <astral/render/renderer.hpp>

namespace astral {

struct GBufferPass {
    explicit GBufferPass(gpu::Device& device);
    ~GBufferPass();
    GBufferPass(const GBufferPass&) = delete;
    GBufferPass& operator=(const GBufferPass&) = delete;

    [[nodiscard]] GBufferTextures add(RenderGraph& graph, const FrameInputs& inputs, RenderStats& stats) const;

private:
    gpu::Device& device_;
    gpu::PipelineHandle pipeline_{};
};

} // namespace astral
