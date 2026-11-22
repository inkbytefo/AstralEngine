#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>

namespace astral {

struct SdfPass {
    explicit SdfPass(gpu::Device& device);
    ~SdfPass();
    SdfPass(const SdfPass&) = delete;
    SdfPass& operator=(const SdfPass&) = delete;

    void add(RenderGraph& graph, const FrameInputs& inputs, GBufferTextures& gbuffer) const;

private:
    gpu::Device& device_;
    gpu::PipelineHandle pipeline_{};
};

} // namespace astral
