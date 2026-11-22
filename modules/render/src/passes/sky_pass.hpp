#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/render/render_graph.hpp>

namespace astral {

/// Draws the optional custom `shader_type sky` background where depth is still at the far plane.
struct SkyPass {
    void add(RenderGraph& graph, const FrameInputs& inputs, RGTexture hdr, RGTexture depth) const;
};

} // namespace astral
