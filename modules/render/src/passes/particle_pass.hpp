#pragma once

#include "passes/frame_inputs.hpp"

#include <unordered_map>

namespace astral {
class ParticlePass {
public:
    ParticlePass(gpu::Device& device, gpu::Format format);
    ~ParticlePass();
    ParticlePass(const ParticlePass&) = delete;
    ParticlePass& operator=(const ParticlePass&) = delete;

    void add(RenderGraph& graph, const FrameInputs& inputs, RGTexture target, const GBufferTextures& gb,
             RGTexture fog = {});

private:
    struct EmitterState {
        gpu::BufferHandle buffer;
        float age = 0;
        std::uint32_t count = 0, seed = 0;
        std::uint64_t seen = 0;
    };
    struct Draw {
        ParticleBurst burst;
        std::uint32_t count;
        std::uint64_t address = 0;
        float previous_age = 0;
        bool reset = false;
    };
    std::vector<Draw> collect(std::span<const ParticleBurst> emitters);
    void simulate(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gb, std::span<const Draw> draws);

    gpu::Device& device_;
    gpu::PipelineHandle pipeline_{}, simulation_{};
    std::unordered_map<std::uint64_t, EmitterState> states_;
    std::uint64_t serial_ = 0;
};
} // namespace astral
