#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>

namespace astral {

/// ACES tonemap with optional bloom compositing and sRGB encoding for UNORM targets.
struct TonemapPass {
    TonemapPass(gpu::Device& device, gpu::Format target_format);
    ~TonemapPass();
    TonemapPass(const TonemapPass&) = delete;
    TonemapPass& operator=(const TonemapPass&) = delete;

    void add(RenderGraph& graph, const FrameInputs& inputs, RGTexture hdr, RGTexture bloom, RGTexture target) const;

private:
    gpu::Device& device_;
    gpu::PipelineHandle pipeline_{};
};

} // namespace astral
