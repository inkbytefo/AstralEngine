#pragma once

#include "passes/denoise_pass.hpp"
#include "passes/frame_inputs.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>

namespace astral {

/// Recombines denoised diffuse/specular lighting with albedo, emission, sky, and visible area lights.
struct CompositePass {
    explicit CompositePass(gpu::Device& device) noexcept : device_(device) {}
    ~CompositePass();
    CompositePass(const CompositePass&) = delete;
    CompositePass& operator=(const CompositePass&) = delete;

    [[nodiscard]] bool ensure_pipelines();
    void destroy_pipelines();

    [[nodiscard]] RGTexture add(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                                const DenoiseOutput& denoised) const;

private:
    gpu::Device& device_;
    gpu::PipelineHandle pipeline_{};
};

} // namespace astral
