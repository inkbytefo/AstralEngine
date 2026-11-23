#pragma once

#include "passes/ddgi_pass.hpp"
#include "passes/frame_inputs.hpp"
#include "stores/shader_store.hpp"

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

/// Ray-traced direct light (with optional spatiotemporal ReSTIR DI), near-field GI, and reflections;
/// owns the temporal geometry history pair and ping-pong ReSTIR reservoir buffers.
struct RtLightingPass {
    explicit RtLightingPass(gpu::Device& device) noexcept : device_(device) {}
    ~RtLightingPass();
    RtLightingPass(const RtLightingPass&) = delete;
    RtLightingPass& operator=(const RtLightingPass&) = delete;

    /// Recreates the geometry history textures and ReSTIR reservoir buffers if dimensions changed.
    [[nodiscard]] bool resize(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] RtLightingOutput add(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                                       const DdgiOutput& ddgi, const RtPipelines& pipelines) const;

private:
    void add_restir(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                    RGTexture geometry_prev, const RtPipelines& pipelines, std::uint64_t prev_res,
                    std::uint64_t temp_res, std::uint64_t out_res) const;

    gpu::Device& device_;
    std::uint32_t width_ = 0, height_ = 0;
    std::array<gpu::TextureHandle, 2> geometry_{};
    std::array<gpu::BufferHandle, 2> reservoirs_{};
    gpu::BufferHandle reservoir_temp_{};
};

} // namespace astral
