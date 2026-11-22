#include "passes/composite_pass.hpp"

#include <shaders/composite.comp.hpp>

namespace astral {

CompositePass::~CompositePass() { destroy_pipelines(); }

bool CompositePass::ensure_pipelines() {
    if (pipeline_.valid()) return true;
    pipeline_ =
        device_.create_compute_pipeline({.shader = {shaders::composite_comp}, .debug_name = "astral.composite"});
    return pipeline_.valid();
}

void CompositePass::destroy_pipelines() {
    device_.destroy(pipeline_);
    pipeline_ = {};
}

RGTexture CompositePass::add(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                             const DenoiseOutput& denoised) const {
    const std::uint32_t w = inputs.extent.width;
    const std::uint32_t h = inputs.extent.height;
    const RGTexture hdr = graph.create("hdr", {w, h, gpu::Format::RGBA16Float});
    const std::uint64_t frame_address = inputs.addresses.frame;
    const std::uint64_t rect_address = inputs.addresses.rects;
    const auto debug_view = static_cast<std::uint32_t>(inputs.settings.debug_view);
    const gpu::PipelineHandle pipeline = pipeline_;

    graph.add_pass(
        "composite",
        [gbuffer, denoised, hdr](PassBuilder& b) {
            read_gbuffer(b, gbuffer);
            for (RGTexture t : {denoised.diffuse, denoised.specular}) b.read(t);
            b.write_storage(hdr);
        },
        [pipeline, frame_address, rect_address, gbuffer, denoised, hdr, debug_view, w, h](PassContext& ctx) {
            const CompositePush push{frame_address,
                                     rect_address,
                                     bindless_gbuffer(ctx, gbuffer),
                                     ctx.bindless(denoised.diffuse),
                                     ctx.bindless(denoised.specular),
                                     ctx.bindless(hdr),
                                     debug_view};
            ctx.cmd.bind_pipeline(pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });
    return hdr;
}

} // namespace astral
