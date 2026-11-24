#pragma once

#include "passes/frame_inputs.hpp"

namespace astral {

Vec4 aerial_parameters(const Environment& environment) noexcept;

class AerialPass {
public:
    explicit AerialPass(gpu::Device& device);
    ~AerialPass();
    AerialPass(const AerialPass&) = delete;
    AerialPass& operator=(const AerialPass&) = delete;

    [[nodiscard]] RGTexture add(RenderGraph& graph, const FrameInputs& inputs, RGTexture hdr,
                                const GBufferTextures& gbuffer) const;

private:
    gpu::Device& device_;
    gpu::PipelineHandle pipeline_{};
};

} // namespace astral
