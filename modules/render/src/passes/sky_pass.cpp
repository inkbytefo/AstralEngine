#include "passes/sky_pass.hpp"

namespace astral {

void SkyPass::add(RenderGraph& graph, const FrameInputs& inputs, RGTexture hdr, const GBufferTextures& gbuffer) const {
    const GpuShader* gs = inputs.materials.shader_of(inputs.scene.sky);
    if (!gs || gs->kind != GpuShader::Kind::Sky) return;
    const gpu::PipelineHandle pipeline = gs->forward;
    const std::uint32_t index = inputs.scene.sky.index;
    const std::uint64_t frame_address = inputs.addresses.frame;
    const std::uint64_t material_address = inputs.addresses.materials;
    graph.add_pass(
        "sky",
        [hdr, gbuffer](PassBuilder& b) {
            b.color(hdr, gpu::LoadOp::Load);
            b.depth(gbuffer.depth, gpu::LoadOp::Load); // hardware depth rejects meshes (depth < 1)
            if (gbuffer.has_sdf) b.read(gbuffer.sdf_depth);
        },
        [pipeline, frame_address, material_address, index, gbuffer](PassContext& ctx) {
            const std::uint32_t sdf_depth = gbuffer.has_sdf ? ctx.bindless(gbuffer.sdf_depth) : no_texture;
            const ScreenPush push{frame_address, material_address, index, sdf_depth};
            ctx.cmd.bind_pipeline(pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.draw(3);
        });
}

} // namespace astral
