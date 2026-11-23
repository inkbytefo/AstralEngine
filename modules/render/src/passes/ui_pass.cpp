#include "passes/ui_pass.hpp"

#include <shaders/ui.frag.hpp>
#include <shaders/ui.vert.hpp>

#include <algorithm>
#include <cmath>

namespace astral {

namespace {

struct UiDrawItem {
    std::uint32_t first_index, index_count;
    std::int32_t vertex_offset;
    std::uint32_t texture; // bindless index or UINT32_MAX
    std::uint32_t extra_flags;
    float sdf_softness;
    std::int32_t x, y;
    std::uint32_t cw, ch;
};

std::vector<UiDrawItem> collect_ui_draws(std::span<const UiLayer> layers, const TextureStore& textures, std::uint32_t w,
                                         std::uint32_t h, std::vector<UiVertex>& vertex_scratch,
                                         std::vector<std::uint32_t>& index_scratch) {
    vertex_scratch.clear();
    index_scratch.clear();
    std::vector<UiDrawItem> draws;
    for (const UiLayer& layer : layers) {
        const auto vertex_base = static_cast<std::int32_t>(vertex_scratch.size());
        const auto index_base = static_cast<std::uint32_t>(index_scratch.size());
        vertex_scratch.insert(vertex_scratch.end(), layer.vertices.begin(), layer.vertices.end());
        index_scratch.insert(index_scratch.end(), layer.indices.begin(), layer.indices.end());
        for (const UiCommand& c : layer.commands) {
            if (c.index_count == 0 || c.first_index + c.index_count > layer.indices.size()) continue;
            const float x0 = std::clamp(c.clip.x, 0.0f, static_cast<float>(w));
            const float y0 = std::clamp(c.clip.y, 0.0f, static_cast<float>(h));
            const float x1 = std::clamp(c.clip.z, 0.0f, static_cast<float>(w));
            const float y1 = std::clamp(c.clip.w, 0.0f, static_cast<float>(h));
            if (x1 <= x0 || y1 <= y0) continue;
            const std::uint32_t texture = textures.bindless_or(c.texture, UINT32_MAX);
            const std::uint32_t extra = (texture != UINT32_MAX && textures.is_linear_sampled(c.texture)) ? 4u : 0u;
            draws.push_back({index_base + c.first_index, c.index_count, vertex_base + c.vertex_offset, texture, extra,
                             c.sdf_softness, static_cast<std::int32_t>(x0), static_cast<std::int32_t>(y0),
                             static_cast<std::uint32_t>(x1 - std::floor(x0)),
                             static_cast<std::uint32_t>(y1 - std::floor(y0))});
        }
    }
    return draws;
}

void record_ui_draws(gpu::CommandList& cmd, gpu::PipelineHandle pipeline, std::uint32_t w, std::uint32_t h,
                     std::uint64_t vertices, std::uint64_t indices, std::uint32_t linear_target,
                     const std::vector<UiDrawItem>& draws) {
    if (draws.empty()) return;
    cmd.bind_pipeline(pipeline);
    for (const UiDrawItem& d : draws) {
        const UiPush push{{2.0f / static_cast<float>(w), 2.0f / static_cast<float>(h)},
                          {-1.0f, -1.0f},
                          vertices,
                          indices,
                          d.vertex_offset,
                          d.texture,
                          (d.texture != UINT32_MAX ? 1u : 0u) | linear_target | d.extra_flags,
                          d.sdf_softness};
        cmd.set_scissor(d.x, d.y, d.cw, d.ch);
        cmd.push_constants(&push, sizeof(push));
        cmd.draw(d.index_count, 1, d.first_index);
    }
}

} // namespace

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
    std::vector<UiDrawItem> draws =
        collect_ui_draws(inputs.scene.ui, inputs.textures, w, h, vertex_scratch_, index_scratch_);
    if (draws.empty()) return;

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
            record_ui_draws(ctx.cmd, pipeline, w, h, vertices, indices, linear_target, draws);
        });
}

void UiPass::record_standalone(gpu::CommandList& cmd, gpu::TextureHandle output, gpu::Extent2D extent,
                               gpu::Format target_format, const TextureStore& textures, std::span<const UiLayer> layers,
                               GrowBuffer& ui_vertices, GrowBuffer& ui_indices, Vec4 clear_color) {
    const std::uint32_t w = extent.width;
    const std::uint32_t h = extent.height;
    const std::vector<UiDrawItem> draws = collect_ui_draws(layers, textures, w, h, vertex_scratch_, index_scratch_);
    std::uint64_t vertices = 0;
    std::uint64_t indices = 0;
    if (!draws.empty()) {
        ui_vertices.write(device_, vertex_scratch_.data(), vertex_scratch_.size() * sizeof(UiVertex));
        ui_indices.write(device_, index_scratch_.data(), index_scratch_.size() * sizeof(std::uint32_t));
        vertices = device_.buffer_address(ui_vertices.buffer);
        indices = device_.buffer_address(ui_indices.buffer);
    }
    const std::uint32_t linear_target = is_srgb(target_format) ? 2u : 0u;
    cmd.transition(output, gpu::ResourceState::ColorTarget);
    cmd.begin_rendering({
        .color = {{.texture = output,
                   .load = gpu::LoadOp::Clear,
                   .store = gpu::StoreOp::Store,
                   .clear = {clear_color.x, clear_color.y, clear_color.z, clear_color.w}}},
        .extent = extent,
    });
    cmd.set_viewport(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h));
    record_ui_draws(cmd, pipeline_, w, h, vertices, indices, linear_target, draws);
    cmd.end_rendering();
}

} // namespace astral
