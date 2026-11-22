#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/render/render_graph.hpp>
#include <astral/render/renderer.hpp>

#include <span>
#include <vector>

namespace astral {

/// Full-screen post-processing material chain (ping-pong after tonemapping).
struct PostPass {
    /// Filters `scene.post` to valid `shader_type post` materials; returned span lives until the next call.
    [[nodiscard]] std::span<const MaterialHandle> collect(const RenderScene& scene, const MaterialStore& materials);

    /// Runs `chain` on `scene_color`; the last effect writes `output`.
    void add(RenderGraph& graph, const FrameInputs& inputs, RGTexture scene_color, RGTexture output,
             std::span<const MaterialHandle> chain) const;

private:
    std::vector<MaterialHandle> chain_scratch_;
};

} // namespace astral
