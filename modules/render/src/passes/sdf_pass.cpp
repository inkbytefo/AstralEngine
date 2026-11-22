#include "passes/sdf_pass.hpp"

#include <shaders/sdf.comp.hpp>

namespace astral {

SdfPass::SdfPass(gpu::Device& device) : device_(device) {
    pipeline_ = device_.create_compute_pipeline({.shader = {shaders::sdf_comp}, .debug_name = "astral.sdf"});
}

SdfPass::~SdfPass() { device_.destroy(pipeline_); }

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

} // namespace astral
