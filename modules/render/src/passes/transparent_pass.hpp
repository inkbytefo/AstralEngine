#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>
#include <astral/render/renderer.hpp>

namespace astral {

/// Forward-lit blended surfaces drawn back-to-front over the tonemapped scene.
struct TransparentPass {
    explicit TransparentPass(gpu::Device& device) noexcept : device_(device) {}

    void add(RenderGraph& graph, const FrameInputs& inputs, RGTexture target, RGTexture depth,
             RenderStats& stats, RGTexture fog = {}) const;

private:
    gpu::Device& device_;
};

} // namespace astral
