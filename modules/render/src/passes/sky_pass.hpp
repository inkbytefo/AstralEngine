#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/render/render_graph.hpp>

namespace astral {

/// Draws the optional custom `shader_type sky` background where neither mesh nor SDF geometry was hit.
struct SkyPass {
    void add(RenderGraph& graph, const FrameInputs& inputs, RGTexture hdr, const GBufferTextures& gbuffer) const;
};

} // namespace astral
