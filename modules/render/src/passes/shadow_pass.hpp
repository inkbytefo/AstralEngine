#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/core/math.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>
#include <astral/render/renderer.hpp>

#include <cstdint>

namespace astral {

struct ShadowFrame {
    Mat4 light_view_projection = Mat4::identity();
    float texel_world = 0.0f;
    bool enabled = false;
    std::uint32_t bindless_index = no_shadow;
};

struct ShadowPass {
    ShadowPass(gpu::Device& device, std::uint32_t map_size);
    ~ShadowPass();
    ShadowPass(const ShadowPass&) = delete;
    ShadowPass& operator=(const ShadowPass&) = delete;

    void resize(std::uint32_t map_size);
    [[nodiscard]] ShadowFrame prepare(const RenderScene& scene, const MeshStore& meshes) const;
    [[nodiscard]] RGTexture add(RenderGraph& graph, const FrameInputs& inputs, const ShadowFrame& shadow,
                                RenderStats& stats) const;

private:
    gpu::Device& device_;
    gpu::PipelineHandle pipeline_{};
    gpu::TextureHandle shadow_map_{};
    std::uint32_t map_size_ = 0;
};

} // namespace astral
