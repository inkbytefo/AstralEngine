#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/render/render_graph.hpp>

namespace astral {

/// Draws the optional custom `shader_type sky` background where neither mesh nor SDF geometry was hit.
class SkyPass {
public:
    explicit SkyPass(gpu::Device& device);
    ~SkyPass();
    SkyPass(const SkyPass&) = delete;
    SkyPass& operator=(const SkyPass&) = delete;

    void bake_environment(RenderGraph& graph, const FrameInputs& inputs) const;
    void add(RenderGraph& graph, const FrameInputs& inputs, RGTexture hdr, const GBufferTextures& gbuffer) const;
    [[nodiscard]] std::uint64_t environment_key(const RenderScene& scene, const RenderSettings& settings,
                                                const MaterialStore& materials) const;

private:
    gpu::Device& device_;
    gpu::PipelineHandle filter_{};
};

} // namespace astral
