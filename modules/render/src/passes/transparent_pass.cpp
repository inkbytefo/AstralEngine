#include "passes/transparent_pass.hpp"

namespace astral {

void TransparentPass::add(RenderGraph& graph, const FrameInputs& inputs, RGTexture target, RGTexture depth,
                          RenderStats& stats, RGTexture fog) const {
    if (inputs.instances.transparent_draws.empty()) return;
    graph.add_pass(
        "transparent",
        [target, depth, fog](PassBuilder& b) {
            if (fog.valid()) b.read(fog);
            b.color(target, gpu::LoadOp::Load);
            b.depth(depth, gpu::LoadOp::Load);
        },
        [this, &inputs, &stats, fog](PassContext& ctx) {
            ShaderHandle bound{};
            for (const TransparentDraw& draw : inputs.instances.transparent_draws) {
                const GpuShader* gs = inputs.shaders.get(draw.shader);
                const GpuMesh* mesh = inputs.meshes.get(draw.mesh);
                if (!gs || !mesh) continue;
                if (draw.shader != bound) {
                    ctx.cmd.bind_pipeline(gs->forward);
                    bound = draw.shader;
                }
                const MeshPush push{device_.buffer_address(mesh->vertices), inputs.addresses.instances,
                                    inputs.addresses.frame, inputs.addresses.materials, draw.instance,
                                    inputs.addresses.params, inputs.scene.time,
                                    fog.valid() ? ctx.bindless(fog) : 0xFFFFFFFFu, inputs.addresses.lights};
                ctx.cmd.push_constants(&push, sizeof(push));
                ctx.cmd.bind_index_buffer(mesh->indices, gpu::IndexType::Uint32);
                ctx.cmd.draw_indexed(mesh->index_count, 1);
                ++stats.draw_calls;
                ++stats.instances;
            }
        });
}

} // namespace astral
