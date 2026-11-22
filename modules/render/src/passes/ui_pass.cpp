#include "passes/ui_pass.hpp"

#include <shaders/ui.frag.hpp>
#include <shaders/ui.vert.hpp>

#include <algorithm>
#include <cmath>

namespace astral {

UiPass::UiPass(gpu::Device& device, gpu::Format target_format) : device_(device) {
    pipeline_ = device_.create_graphics_pipeline({
        .vertex = {shaders::ui_vert},
        .fragment = {shaders::ui_frag},
        .color_formats = {target_format},
        .depth = {.format = gpu::Format::Undefined, .test = false, .write = false},
        .alpha_blend = true,
        .debug_name = "astral.ui",
    });
}

UiPass::~UiPass() { device_.destroy(pipeline_); }

void UiPass::add(RenderGraph& graph, const FrameInputs& inputs, RGTexture output) {
    const std::uint32_t w = inputs.extent.width;
    const std::uint32_t h = inputs.extent.height;
    std::size_t commands = 0;
    vertex_scratch_.clear();
    index_scratch_.clear();
    struct Draw {
        std::uint32_t first_index, index_count;
        std::int32_t vertex_offset;
        std::uint32_t texture; // bindless index or UINT32_MAX
        float sdf_softness;
        std::int32_t x, y;
        std::uint32_t cw, ch;
    };
    std::vector<Draw> draws;
    for (const UiLayer& layer : inputs.scene.ui) {
        const auto vertex_base = static_cast<std::int32_t>(vertex_scratch_.size());
        const auto index_base = static_cast<std::uint32_t>(index_scratch_.size());
        vertex_scratch_.insert(vertex_scratch_.end(), layer.vertices.begin(), layer.vertices.end());
        index_scratch_.insert(index_scratch_.end(), layer.indices.begin(), layer.indices.end());
        for (const UiCommand& c : layer.commands) {
            if (c.index_count == 0 || c.first_index + c.index_count > layer.indices.size()) continue;
            const float x0 = std::clamp(c.clip.x, 0.0f, static_cast<float>(w));
            const float y0 = std::clamp(c.clip.y, 0.0f, static_cast<float>(h));
            const float x1 = std::clamp(c.clip.z, 0.0f, static_cast<float>(w));
            const float y1 = std::clamp(c.clip.w, 0.0f, static_cast<float>(h));
            if (x1 <= x0 || y1 <= y0) continue;
            const std::uint32_t texture = inputs.textures.bindless_or(c.texture, UINT32_MAX);
            draws.push_back({index_base + c.first_index, c.index_count, vertex_base + c.vertex_offset, texture,
                             c.sdf_softness, static_cast<std::int32_t>(x0), static_cast<std::int32_t>(y0),
                             static_cast<std::uint32_t>(x1 - std::floor(x0)),
                             static_cast<std::uint32_t>(y1 - std::floor(y0))});
            ++commands;
        }
    }
    if (commands == 0) return;

    inputs.slot.ui_vertices.write(device_, vertex_scratch_.data(), vertex_scratch_.size() * sizeof(UiVertex));
    inputs.slot.ui_indices.write(device_, index_scratch_.data(), index_scratch_.size() * sizeof(std::uint32_t));
    const std::uint64_t vertices = device_.buffer_address(inputs.slot.ui_vertices.buffer);
    const std::uint64_t indices = device_.buffer_address(inputs.slot.ui_indices.buffer);
    const std::uint32_t linear_target = is_srgb(inputs.target_format) ? 2u : 0u;
    const gpu::PipelineHandle pipeline = pipeline_;

    graph.add_pass(
        "ui",
        [output](PassBuilder& b) { b.color(output, gpu::LoadOp::Load); },
        [pipeline, w, h, vertices, indices, linear_target, draws = std::move(draws)](PassContext& ctx) {
            ctx.cmd.bind_pipeline(pipeline);
            for (const Draw& d : draws) {
                const UiPush push{{2.0f / static_cast<float>(w), 2.0f / static_cast<float>(h)},
                                  {-1.0f, -1.0f},
                                  vertices,
                                  indices,
                                  d.vertex_offset,
                                  d.texture,
                                  (d.texture != UINT32_MAX ? 1u : 0u) | linear_target,
                                  d.sdf_softness};
                ctx.cmd.set_scissor(d.x, d.y, d.cw, d.ch);
                ctx.cmd.push_constants(&push, sizeof(push));
                ctx.cmd.draw(d.index_count, 1, d.first_index);
            }
        });
}

} // namespace astral
