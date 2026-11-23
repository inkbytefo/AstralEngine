#include "passes/path_tracer_pass.hpp"

#include <algorithm>

namespace astral {

PathTracerPass::~PathTracerPass() {
    for (auto& t : accum_) device_.destroy(t);
}

bool PathTracerPass::resize(std::uint32_t width, std::uint32_t height) {
    if (width_ == width && height_ == height) return false;
    for (auto& t : accum_) {
        device_.destroy(t);
        t = device_.create_texture({.width = width,
                                    .height = height,
                                    .format = gpu::Format::RGBA32Float,
                                    .usage = gpu::TextureUsage::Sampled | gpu::TextureUsage::Storage,
                                    .debug_name = "astral.path_tracer.accum"});
    }
    width_ = width;
    height_ = height;
    sample_index_ = 0;
    return true;
}

RGTexture PathTracerPass::add(RenderGraph& graph, const FrameInputs& inputs, gpu::PipelineHandle pipeline) {
    const std::uint32_t w = inputs.extent.width;
    const std::uint32_t h = inputs.extent.height;
    const std::uint32_t parity = sample_index_ & 1u;
    const std::uint32_t prev = parity ^ 1u;
    const std::uint32_t sample = sample_index_++;
    const std::uint32_t max_bounces = std::clamp(inputs.settings.path_tracer_max_bounces, 1u, 16u);

    const RGTexture accum_prev = graph.import(accum_[prev], "pt.accum.prev");
    const RGTexture accum_curr = graph.import(accum_[parity], "pt.accum");
    const RGTexture out_hdr = graph.create("pt.hdr", {w, h, gpu::Format::RGBA16Float});
    const std::uint64_t frame_address = inputs.addresses.frame;

    graph.add_pass(
        "path_tracer",
        [accum_prev, accum_curr, out_hdr](PassBuilder& b) {
            b.read(accum_prev);
            b.write_storage(accum_curr);
            b.write_storage(out_hdr);
        },
        [pipeline, frame_address, accum_prev, accum_curr, out_hdr, sample, max_bounces, w, h](PassContext& ctx) {
            const PathTracerPush push{frame_address,           ctx.bindless(accum_prev), ctx.bindless(accum_curr),
                                      ctx.bindless(out_hdr),   sample,                   max_bounces};
            ctx.cmd.bind_pipeline(pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });
    return out_hdr;
}

} // namespace astral
