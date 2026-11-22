#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>

#include <array>
#include <cstdint>

namespace astral {

/// Temporal anti-aliasing with Catmull-Rom history lookup and YCoCg variance clipping.
struct TaaPass {
    explicit TaaPass(gpu::Device& device) noexcept : device_(device) {}
    ~TaaPass();
    TaaPass(const TaaPass&) = delete;
    TaaPass& operator=(const TaaPass&) = delete;

    [[nodiscard]] bool ensure_pipelines();
    void destroy_pipelines();

    /// Recreates the ping-pong TAA history textures if dimensions changed. Returns true when resized.
    [[nodiscard]] bool resize(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] RGTexture add(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                                RGTexture hdr) const;

private:
    gpu::Device& device_;
    gpu::PipelineHandle pipeline_{};
    std::uint32_t width_ = 0, height_ = 0;
    std::array<gpu::TextureHandle, 2> history_{};
};

} // namespace astral
