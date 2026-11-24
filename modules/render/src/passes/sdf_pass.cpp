#include "passes/sdf_pass.hpp"

#include <shaders/sdf.comp.hpp>
#include <shaders/sdf_depth.frag.hpp>
#include <shaders/tonemap.vert.hpp>

namespace astral {

SdfPass::SdfPass(gpu::Device& device) : device_(device) {
    pipeline_ = device_.create_compute_pipeline({.shader = {shaders::sdf_comp}, .debug_name = "astral.sdf"});
    depth_pipeline_ = device_.create_graphics_pipeline({
        .vertex = {shaders::tonemap_vert},
        .fragment = {shaders::sdf_depth_frag},
        .depth = {.format = gpu::Format::D32Float, .test = true, .write = true, .compare = gpu::CompareOp::Less},
        .debug_name = "astral.sdf_depth",
    });
}

SdfPass::~SdfPass() {
    device_.destroy(pipeline_);
    device_.destroy(depth_pipeline_);
}

void SdfPass::add(RenderGraph& graph, const FrameInputs& inputs, GBufferTextures& gbuffer) const {
    if (inputs.instances.shapes.empty()) return;
    const std::uint32_t w = inputs.extent.width;
    const std::uint32_t h = inputs.extent.height;
    gbuffer.sdf_albedo = graph.create("sdf.albedo", {w, h, gpu::Format::RGBA8Unorm});
    gbuffer.sdf_normal = graph.create("sdf.normal", {w, h, gpu::Format::RGBA16Float});
    gbuffer.sdf_depth = graph.create("sdf.depth", {w, h, gpu::Format::R32Float});
    gbuffer.sdf_velocity = graph.create("sdf.velocity", {w, h, gpu::Format::RG16Float});
    gbuffer.has_sdf = true;

    const std::uint64_t frame_address = inputs.addresses.frame;
    const std::uint64_t shape_address = inputs.addresses.shapes;
    const gpu::PipelineHandle pipeline = pipeline_;
    const GBufferTextures g = gbuffer;
    graph.add_pass(
        "sdf",
        [g](PassBuilder& b) {
            b.read(g.depth);
            b.write_storage(g.sdf_albedo);
            b.write_storage(g.sdf_normal);
            b.write_storage(g.sdf_depth);
            b.write_storage(g.sdf_velocity);
        },
        [pipeline, frame_address, shape_address, g, w, h](PassContext& ctx) {
            const SdfPush push{frame_address, shape_address, ctx.bindless(g.depth), ctx.bindless(g.sdf_albedo),
                               ctx.bindless(g.sdf_normal), ctx.bindless(g.sdf_depth), ctx.bindless(g.sdf_velocity)};
            ctx.cmd.bind_pipeline(pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });
}

void SdfPass::merge_depth(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer) const {
    if (!gbuffer.has_sdf || !depth_pipeline_.valid() ||
        (inputs.instances.transparent_draws.empty() && inputs.scene.debug_lines.empty())) return;

    const gpu::PipelineHandle pipeline = depth_pipeline_;
    graph.add_pass(
        "sdf.depth_merge",
        [gbuffer](PassBuilder& b) {
            b.read(gbuffer.sdf_depth);
            b.depth(gbuffer.depth, gpu::LoadOp::Load);
        },
        [pipeline, sdf_depth = gbuffer.sdf_depth](PassContext& ctx) {
            const std::uint32_t depth_index = ctx.bindless(sdf_depth);
            ctx.cmd.bind_pipeline(pipeline);
            ctx.cmd.push_constants(&depth_index, sizeof(depth_index));
            ctx.cmd.draw(3);
        });
}

} // namespace astral
