#include "passes/tonemap_pass.hpp"

#include <shaders/tonemap.frag.hpp>
#include <shaders/tonemap.vert.hpp>

namespace astral {

TonemapPass::TonemapPass(gpu::Device& device, gpu::Format target_format) : device_(device) {
    pipeline_ = device_.create_graphics_pipeline({
        .vertex = {shaders::tonemap_vert},
        .fragment = {shaders::tonemap_frag},
        .color_formats = {target_format},
        .depth = {.format = gpu::Format::Undefined, .test = false, .write = false},
        .debug_name = "astral.tonemap",
    });
}

TonemapPass::~TonemapPass() { device_.destroy(pipeline_); }

void TonemapPass::add(RenderGraph& graph, const FrameInputs& inputs, RGTexture hdr, RGTexture bloom,
                      RGTexture target) const {
    const gpu::PipelineHandle pipeline = pipeline_;
    const float exposure = inputs.scene.environment.exposure;
    const std::uint32_t encode_srgb = is_srgb(inputs.target_format) ? 0u : 1u;
    const float bloom_strength = inputs.settings.bloom_strength;
    graph.add_pass(
        "tonemap",
        [hdr, bloom, target](PassBuilder& b) {
            b.read(hdr);
            if (bloom.valid()) b.read(bloom);
            b.color(target, gpu::LoadOp::DontCare);
        },
        [pipeline, hdr, bloom, exposure, encode_srgb, bloom_strength](PassContext& ctx) {
            TonemapPush push{ctx.bindless(hdr), exposure, encode_srgb};
            if (bloom.valid()) {
                push.bloom = ctx.bindless(bloom);
                push.bloom_strength = bloom_strength;
            }
            ctx.cmd.bind_pipeline(pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.draw(3);
        });
}

} // namespace astral
