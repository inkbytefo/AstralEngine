#pragma once

#include "passes/ddgi_pass.hpp"
#include "passes/frame_inputs.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>

#include <array>
#include <cstdint>

namespace astral {

struct RtLightingOutput {
    RGTexture diffuse{};
    RGTexture specular{};
    RGTexture geometry{};
    RGTexture geometry_prev{};
};

/// Ray-traced direct light, near-field GI, and reflections; owns the temporal geometry history pair.
struct RtLightingPass {
    explicit RtLightingPass(gpu::Device& device) noexcept : device_(device) {}
    ~RtLightingPass();
    RtLightingPass(const RtLightingPass&) = delete;
    RtLightingPass& operator=(const RtLightingPass&) = delete;

    /// Recreates the geometry history textures if dimensions changed. Returns true when resized.
    [[nodiscard]] bool resize(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] RtLightingOutput add(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                                       const DdgiOutput& ddgi, gpu::PipelineHandle lighting_pipeline) const;

private:
    gpu::Device& device_;
    std::uint32_t width_ = 0, height_ = 0;
    std::array<gpu::TextureHandle, 2> geometry_{};
};

} // namespace astral
