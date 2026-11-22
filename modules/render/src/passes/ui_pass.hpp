#pragma once

#include "passes/frame_inputs.hpp"

#include <astral/gpu/device.hpp>
#include <astral/render/render_graph.hpp>
#include <astral/render/ui.hpp>

#include <cstdint>
#include <vector>

namespace astral {

/// 2D overlay (debug UI, HUD, text, sprites) rendered over the final image.
struct UiPass {
    UiPass(gpu::Device& device, gpu::Format target_format);
    ~UiPass();
    UiPass(const UiPass&) = delete;
    UiPass& operator=(const UiPass&) = delete;

    void add(RenderGraph& graph, const FrameInputs& inputs, RGTexture output);

private:
    gpu::Device& device_;
    gpu::PipelineHandle pipeline_{};
    std::vector<UiVertex> vertex_scratch_;
    std::vector<std::uint32_t> index_scratch_;
};

} // namespace astral
