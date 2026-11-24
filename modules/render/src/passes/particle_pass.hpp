#pragma once

#include "passes/frame_inputs.hpp"

namespace astral {
class ParticlePass {
public:
    ParticlePass(gpu::Device& device, gpu::Format format);
    ~ParticlePass();
    ParticlePass(const ParticlePass&) = delete;
    ParticlePass& operator=(const ParticlePass&) = delete;

    void add(RenderGraph& graph, const FrameInputs& inputs, RGTexture target, const GBufferTextures& gb,
             RGTexture fog = {}) const;

private:
    gpu::Device& device_;
    gpu::PipelineHandle pipeline_{};
};
} // namespace astral
