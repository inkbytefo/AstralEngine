#include "passes/bloom_pass.hpp"

#include <shaders/bloom_down.comp.hpp>
#include <shaders/bloom_up.comp.hpp>

#include <algorithm>

namespace astral {

BloomPass::~BloomPass() { destroy_pipelines(); }

bool BloomPass::ensure_pipelines() {
    if (down_pipeline_.valid() && up_pipeline_.valid()) return true;
    down_pipeline_ =
        device_.create_compute_pipeline({.shader = {shaders::bloom_down_comp}, .debug_name = "astral.bloom_down"});
    up_pipeline_ =
        device_.create_compute_pipeline({.shader = {shaders::bloom_up_comp}, .debug_name = "astral.bloom_up"});
    if (!down_pipeline_.valid() || !up_pipeline_.valid()) {
        destroy_pipelines();
        return false;
    }
    return true;
}

void BloomPass::destroy_pipelines() {
    device_.destroy(down_pipeline_);
    device_.destroy(up_pipeline_);
    down_pipeline_ = {};
    up_pipeline_ = {};
}

BloomPass::DownChain BloomPass::add_downsample(RenderGraph& graph, gpu::Extent2D extent, RGTexture source) const {
    DownChain chain;
    const gpu::PipelineHandle pipeline = down_pipeline_;
    for (std::uint32_t i = 0; i < max_levels; ++i) {
        const std::uint32_t lw = std::max(1u, extent.width >> (i + 1));
        const std::uint32_t lh = std::max(1u, extent.height >> (i + 1));
        chain.sizes[i] = {lw, lh};
        chain.textures[i] = graph.create("bloom.down", {lw, lh, gpu::Format::RGBA16Float});
        const RGTexture src = i == 0 ? source : chain.textures[i - 1];
        const gpu::Extent2D src_size = i == 0 ? extent : chain.sizes[i - 1];
        const RGTexture dst = chain.textures[i];
        graph.add_pass(
            "bloom.down",
            [src, dst](PassBuilder& b) {
                b.read(src);
                b.write_storage(dst);
            },
            [pipeline, src, dst, src_size, lw, lh, i](PassContext& ctx) {
                const BloomDownPush push{
                    ctx.bindless(src),
                    ctx.bindless(dst),
                    {1.0f / static_cast<float>(src_size.width), 1.0f / static_cast<float>(src_size.height)},
                    {lw, lh},
                    i == 0 ? 1u : 0u};
                ctx.cmd.bind_pipeline(pipeline);
                ctx.cmd.push_constants(&push, sizeof(push));
                ctx.cmd.dispatch((lw + 7) / 8, (lh + 7) / 8);
            });
        ++chain.levels;
        if (lw == 1 && lh == 1) break;
    }
    return chain;
}

RGTexture BloomPass::add_upsample(RenderGraph& graph, const DownChain& chain) const {
    const gpu::PipelineHandle pipeline = up_pipeline_;
    RGTexture low = chain.textures[chain.levels - 1];
    for (int i = static_cast<int>(chain.levels) - 2; i >= 0; --i) {
        const auto li = static_cast<std::uint32_t>(i);
        const RGTexture dst =
            graph.create("bloom.up", {chain.sizes[li].width, chain.sizes[li].height, gpu::Format::RGBA16Float});
        const RGTexture current = chain.textures[li];
        const RGTexture lower = low;
        const gpu::Extent2D low_size = chain.sizes[li + 1];
        const gpu::Extent2D size = chain.sizes[li];
        const auto step = static_cast<float>(chain.levels - 1 - li);
        const float blend = step / (step + 1.0f); // equal 1/N weight across all N pyramid scales (unit DC gain)
        graph.add_pass(
            "bloom.up",
            [lower, current, dst](PassBuilder& b) {
                b.read(lower);
                b.read(current);
                b.write_storage(dst);
            },
            [pipeline, lower, current, dst, low_size, size, blend](PassContext& ctx) {
                const BloomUpPush push{
                    ctx.bindless(lower),
                    ctx.bindless(current),
                    ctx.bindless(dst),
                    {1.0f / static_cast<float>(low_size.width), 1.0f / static_cast<float>(low_size.height)},
                    {size.width, size.height},
                    1.0f,
                    blend};
                ctx.cmd.bind_pipeline(pipeline);
                ctx.cmd.push_constants(&push, sizeof(push));
                ctx.cmd.dispatch((size.width + 7) / 8, (size.height + 7) / 8);
            });
        low = dst;
    }
    return low;
}

RGTexture BloomPass::add(RenderGraph& graph, const FrameInputs& inputs, RGTexture source) const {
    if (inputs.settings.bloom_strength <= 0.0f) return {};
    const DownChain chain = add_downsample(graph, inputs.extent, source);
    return chain.levels > 0 ? add_upsample(graph, chain) : RGTexture{};
}

} // namespace astral
