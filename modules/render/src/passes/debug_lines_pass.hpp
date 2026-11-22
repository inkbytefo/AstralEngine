#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>

#include <vector>

namespace astral {

/// Debug lines over the final image: depth-tested lines against scene depth, then on-top lines.
struct DebugLinesPass {
    DebugLinesPass(gpu::Device& device, gpu::Format target_format);
    ~DebugLinesPass();
    DebugLinesPass(const DebugLinesPass&) = delete;
    DebugLinesPass& operator=(const DebugLinesPass&) = delete;

    void add(RenderGraph& graph, const FrameInputs& inputs, RGTexture output, RGTexture depth);

private:
    gpu::Device& device_;
    gpu::PipelineHandle depth_pipeline_{};
    gpu::PipelineHandle top_pipeline_{};
    std::vector<float> vertices_scratch_; // position.xyzw, color per vertex
};

} // namespace astral
