#pragma once

#include "passes/ddgi_pass.hpp"
#include "passes/frame_inputs.hpp"

namespace astral {

Vec4 fog_parameters(const RenderScene& scene) noexcept;

class FogPass {
public:
    explicit FogPass(gpu::Device& device);
    ~FogPass();
    FogPass(const FogPass&) = delete;
    FogPass& operator=(const FogPass&) = delete;

    RGTexture integrate(RenderGraph& graph, const FrameInputs& inputs, RGTexture shadow, RenderStats& stats, DdgiOutput ddgi = {}) const;
    RGTexture composite(RenderGraph& graph, const FrameInputs& inputs, RGTexture hdr,
                        const GBufferTextures& gbuffer, RGTexture atlas) const;

private:
    gpu::Device& device_;
    gpu::PipelineHandle integrate_{}, composite_{}, volume_shadow_{}, probe_reduce_{};
};

} // namespace astral
