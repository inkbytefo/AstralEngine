#include "passes/post_pass.hpp"

namespace astral {

std::span<const MaterialHandle> PostPass::collect(const RenderScene& scene, const MaterialStore& materials) {
    chain_scratch_.clear();
    for (MaterialHandle p : scene.post) {
        if (materials.is_post(p)) chain_scratch_.push_back(p);
    }
    return chain_scratch_;
}

void PostPass::add(RenderGraph& graph, const FrameInputs& inputs, RGTexture scene_color, RGTexture output,
                   std::span<const MaterialHandle> chain) const {
    const std::uint32_t w = inputs.extent.width;
    const std::uint32_t h = inputs.extent.height;
    const std::uint64_t frame_address = inputs.addresses.frame;
    const std::uint64_t material_address = inputs.addresses.materials;
    RGTexture source = scene_color;
    for (std::size_t i = 0; i < chain.size(); ++i) {
        const GpuShader* gs = inputs.materials.shader_of(chain[i]);
        if (!gs) continue;
        const bool last = i + 1 == chain.size();
        const RGTexture target = last ? output : graph.create("post", {w, h, inputs.target_format});
        const gpu::PipelineHandle pipeline = gs->forward;
        const std::uint32_t index = chain[i].index;
        const RGTexture src = source;
        graph.add_pass(
            "post",
            [src, target](PassBuilder& b) {
                b.read(src);
                b.color(target, gpu::LoadOp::DontCare);
            },
            [pipeline, frame_address, material_address, index, src](PassContext& ctx) {
                const ScreenPush push{frame_address, material_address, index, ctx.bindless(src)};
                ctx.cmd.bind_pipeline(pipeline);
                ctx.cmd.push_constants(&push, sizeof(push));
                ctx.cmd.draw(3);
            });
        source = target;
    }
}

} // namespace astral
