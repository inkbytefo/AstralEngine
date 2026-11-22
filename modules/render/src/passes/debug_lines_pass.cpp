#include "passes/debug_lines_pass.hpp"

#include <shaders/debug_line.frag.hpp>
#include <shaders/debug_line.vert.hpp>

namespace astral {

DebugLinesPass::DebugLinesPass(gpu::Device& device, gpu::Format target_format) : device_(device) {
    for (const bool on_top : {false, true}) {
        (on_top ? top_pipeline_ : depth_pipeline_) = device_.create_graphics_pipeline({
            .vertex = {shaders::debug_line_vert},
            .fragment = {shaders::debug_line_frag},
            .color_formats = {target_format},
            .depth = {.format = gpu::Format::D32Float,
                      .test = !on_top,
                      .write = false,
                      .compare = gpu::CompareOp::LessEqual},
            .topology = gpu::Topology::LineList,
            .alpha_blend = true,
            .debug_name = on_top ? "astral.debug_lines_top" : "astral.debug_lines",
        });
    }
}

DebugLinesPass::~DebugLinesPass() {
    device_.destroy(depth_pipeline_);
    device_.destroy(top_pipeline_);
}

void DebugLinesPass::add(RenderGraph& graph, const FrameInputs& inputs, RGTexture output, RGTexture depth) {
    const std::span<const DebugLine> lines = inputs.scene.debug_lines;
    if (lines.empty()) return;

    vertices_scratch_.clear();
    std::uint32_t tested = 0;
    for (const bool on_top : {false, true}) { // depth-tested lines first, then the rest
        for (const DebugLine& l : lines) {
            if (l.on_top != on_top) continue;
            for (const Vec3& p : {l.a, l.b}) {
                vertices_scratch_.insert(vertices_scratch_.end(),
                                         {p.x, p.y, p.z, 1.0f, l.color.x, l.color.y, l.color.z, l.color.w});
            }
            if (!on_top) tested += 2;
        }
    }
    const auto total = static_cast<std::uint32_t>(vertices_scratch_.size() / 8);
    inputs.slot.debug_lines.write(device_, vertices_scratch_.data(), vertices_scratch_.size() * sizeof(float));
    const DebugLinePush push{inputs.view_projection, device_.buffer_address(inputs.slot.debug_lines.buffer)};
    const gpu::PipelineHandle depth_pipeline = depth_pipeline_;
    const gpu::PipelineHandle top_pipeline = top_pipeline_;

    graph.add_pass(
        "debug.lines",
        [output, depth](PassBuilder& b) {
            b.color(output, gpu::LoadOp::Load);
            b.depth(depth, gpu::LoadOp::Load);
        },
        [depth_pipeline, top_pipeline, push, tested, total](PassContext& ctx) {
            if (tested) {
                ctx.cmd.bind_pipeline(depth_pipeline);
                ctx.cmd.push_constants(&push, sizeof(push));
                ctx.cmd.draw(tested);
            }
            if (total > tested) {
                ctx.cmd.bind_pipeline(top_pipeline);
                ctx.cmd.push_constants(&push, sizeof(push));
                ctx.cmd.draw(total - tested, 1, tested);
            }
        });
}

} // namespace astral
