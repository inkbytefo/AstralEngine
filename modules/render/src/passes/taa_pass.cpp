#include "passes/taa_pass.hpp"

#include <shaders/taa.comp.hpp>

#include <algorithm>

namespace astral {

TaaPass::~TaaPass() {
    for (auto& t : history_) device_.destroy(t);
    destroy_pipelines();
}

bool TaaPass::ensure_pipelines() {
    if (pipeline_.valid()) return true;
    pipeline_ = device_.create_compute_pipeline({.shader = {shaders::taa_comp}, .debug_name = "astral.taa"});
    return pipeline_.valid();
}

void TaaPass::destroy_pipelines() {
    device_.destroy(pipeline_);
    pipeline_ = {};
}

bool TaaPass::resize(std::uint32_t width, std::uint32_t height) {
    if (width_ == width && height_ == height) return false;
    for (auto& t : history_) {
        device_.destroy(t);
        t = device_.create_texture({.width = width,
                                    .height = height,
                                    .format = gpu::Format::RGBA16Float,
                                    .usage = gpu::TextureUsage::Sampled | gpu::TextureUsage::Storage,
                                    .debug_name = "astral.history.taa"});
    }
    // A failed allocation must not be committed as the new size: the same size would then never be
    // tried again and the invalid textures would stay. Size 0 makes the next frame retry.
    const bool ok = history_[0].valid() && history_[1].valid();
    width_ = ok ? width : 0;
    height_ = ok ? height : 0;
    return true;
}

RGTexture TaaPass::add(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                       RGTexture hdr) const {
    const std::uint32_t w = inputs.extent.width;
    const std::uint32_t h = inputs.extent.height;
    const std::uint32_t parity = inputs.frame_index & 1u;
    const std::uint32_t prev = parity ^ 1u;
    const RGTexture taa_out = graph.import(history_[parity], "history.taa");
    const RGTexture taa_prev = graph.import(history_[prev], "history.taa.prev");
    // Keep transparent surfaces out of the history, which only has opaque motion vectors.
    const RGTexture composite = inputs.instances.transparent_draws.empty()
                                    ? taa_out
                                    : graph.create("taa.composite", {w, h, gpu::Format::RGBA16Float});

    const std::uint64_t frame_address = inputs.addresses.frame;
    const float blend = std::clamp(inputs.settings.taa_blend, 0.01f, 1.0f);
    const gpu::PipelineHandle pipeline = pipeline_;
    graph.add_pass(
        "taa",
        [gbuffer, hdr, taa_prev, taa_out, composite](PassBuilder& b) {
            read_gbuffer(b, gbuffer);
            b.read(hdr);
            b.read(taa_prev);
            b.write_storage(taa_out);
            if (composite != taa_out) b.write_storage(composite);
        },
        [pipeline, frame_address, gbuffer, hdr, taa_prev, taa_out, composite, blend, w, h](PassContext& ctx) {
            const TaaPush push{frame_address, bindless_gbuffer(ctx, gbuffer), ctx.bindless(hdr), ctx.bindless(taa_prev),
                               ctx.bindless(taa_out), blend, ctx.bindless(composite)};
            ctx.cmd.bind_pipeline(pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });
    return composite;
}

} // namespace astral
