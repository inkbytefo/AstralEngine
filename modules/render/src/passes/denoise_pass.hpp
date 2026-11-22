#pragma once

#include "passes/frame_inputs.hpp"
#include "passes/rt_lighting_pass.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>

#include <array>
#include <cstdint>

namespace astral {

struct DenoiseOutput {
    RGTexture diffuse{};
    RGTexture specular{};
};

/// SVGF-style denoiser: temporal accumulation followed by edge-stopping à-trous wavelet iterations.
struct DenoisePass {
    explicit DenoisePass(gpu::Device& device) noexcept : device_(device) {}
    ~DenoisePass();
    DenoisePass(const DenoisePass&) = delete;
    DenoisePass& operator=(const DenoisePass&) = delete;

    [[nodiscard]] bool ensure_pipelines();
    void destroy_pipelines();

    /// Recreates temporal diffuse/specular/moments history pairs if dimensions changed. Returns true when resized.
    [[nodiscard]] bool resize(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] DenoiseOutput add(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                                    const RtLightingOutput& rt) const;

private:
    struct TemporalOutput {
        RGTexture diffuse{};
        RGTexture specular{};
        RGTexture moments{};
    };

    [[nodiscard]] TemporalOutput add_temporal(RenderGraph& graph, const FrameInputs& inputs,
                                              const GBufferTextures& gbuffer, const RtLightingOutput& rt) const;
    [[nodiscard]] DenoiseOutput add_atrous(RenderGraph& graph, const FrameInputs& inputs,
                                           const GBufferTextures& gbuffer, RGTexture geometry,
                                           const TemporalOutput& temporal) const;

    gpu::Device& device_;
    gpu::PipelineHandle temporal_pipeline_{};
    gpu::PipelineHandle atrous_pipeline_{};
    std::uint32_t width_ = 0, height_ = 0;
    std::array<gpu::TextureHandle, 2> diffuse_{};
    std::array<gpu::TextureHandle, 2> specular_{};
    std::array<gpu::TextureHandle, 2> moments_{};
};

} // namespace astral
