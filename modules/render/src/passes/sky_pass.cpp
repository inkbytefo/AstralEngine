#include "passes/sky_pass.hpp"

namespace astral {

void SkyPass::add(RenderGraph& graph, const FrameInputs& inputs, RGTexture hdr, RGTexture depth) const {
    const GpuShader* gs = inputs.materials.shader_of(inputs.scene.sky);
    if (!gs || gs->kind != GpuShader::Kind::Sky) return;
    const gpu::PipelineHandle pipeline = gs->forward;
    const std::uint32_t index = inputs.scene.sky.index;
    const std::uint64_t frame_address = inputs.addresses.frame;
    const std::uint64_t material_address = inputs.addresses.materials;
    graph.add_pass(
        "sky",
        [hdr, depth](PassBuilder& b) {
            b.color(hdr, gpu::LoadOp::Load);
            b.depth(depth, gpu::LoadOp::Load); // only where nothing was drawn (depth 1)
        },
        [pipeline, frame_address, material_address, index](PassContext& ctx) {
            const ScreenPush push{frame_address, material_address, index, 0};
            ctx.cmd.bind_pipeline(pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.draw(3);
        });
}

} // namespace astral
