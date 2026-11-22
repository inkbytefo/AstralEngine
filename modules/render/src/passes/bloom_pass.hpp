#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>

#include <array>
#include <cstdint>

namespace astral {

/// Half-resolution mip-chain downsample + tent-filtered upsample bloom.
struct BloomPass {
    explicit BloomPass(gpu::Device& device) noexcept : device_(device) {}
    ~BloomPass();
    BloomPass(const BloomPass&) = delete;
    BloomPass& operator=(const BloomPass&) = delete;

    [[nodiscard]] bool ensure_pipelines();
    void destroy_pipelines();

    [[nodiscard]] RGTexture add(RenderGraph& graph, const FrameInputs& inputs, RGTexture source) const;

private:
    static constexpr std::uint32_t max_levels = 6;

    struct DownChain {
        std::array<RGTexture, max_levels> textures{};
        std::array<gpu::Extent2D, max_levels> sizes{};
        std::uint32_t levels = 0;
    };

    [[nodiscard]] DownChain add_downsample(RenderGraph& graph, gpu::Extent2D extent, RGTexture source) const;
    [[nodiscard]] RGTexture add_upsample(RenderGraph& graph, const DownChain& chain) const;

    gpu::Device& device_;
    gpu::PipelineHandle down_pipeline_{};
    gpu::PipelineHandle up_pipeline_{};
};

} // namespace astral
