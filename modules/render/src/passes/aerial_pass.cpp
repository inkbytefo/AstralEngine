#include "passes/aerial_pass.hpp"

#include <shaders/aerial.comp.hpp>

#include <cmath>

namespace astral {

Vec4 aerial_parameters(const Environment& env) noexcept {
    const auto positive = [](float v) { return std::isfinite(v) && v > 0.0f ? v : 0.0f; };
    return {positive(env.aerial_extinction), positive(env.aerial_start_m), 0, 0};
}

AerialPass::AerialPass(gpu::Device& device) : device_(device) {
    pipeline_ = device_.create_compute_pipeline({.shader = {shaders::aerial_comp}, .debug_name = "astral.aerial"});
}

AerialPass::~AerialPass() { device_.destroy(pipeline_); }

RGTexture AerialPass::add(RenderGraph& graph, const FrameInputs& inputs, RGTexture hdr,
                          const GBufferTextures& gbuffer) const {
    if (aerial_parameters(inputs.scene.environment).x == 0 || !pipeline_.valid()) return hdr;

    const auto [w, h] = inputs.extent;
    const RGTexture result = graph.create("aerial.hdr", {w, h, gpu::Format::RGBA16Float});
    const auto frame = inputs.addresses.frame;
    graph.add_pass(
        "aerial perspective",
        [hdr, gbuffer, result](PassBuilder& builder) {
            builder.read(hdr);
            builder.read(gbuffer.depth);
            if (gbuffer.has_sdf) builder.read(gbuffer.sdf_depth);
            builder.write_storage(result);
        },
        [pipeline = pipeline_, frame, hdr, gbuffer, result, w, h](PassContext& ctx) {
            struct Push {
                std::uint64_t frame;
                std::uint32_t hdr, depth, sdf_depth, result;
            };
            const Push push{frame, ctx.bindless(hdr), ctx.bindless(gbuffer.depth),
                            gbuffer.has_sdf ? ctx.bindless(gbuffer.sdf_depth) : 0xFFFFFFFFu,
                            ctx.bindless(result)};
            ctx.cmd.bind_pipeline(pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });
    return result;
}

} // namespace astral
