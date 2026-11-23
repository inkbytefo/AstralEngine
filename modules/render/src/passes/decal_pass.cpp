#include "passes/decal_pass.hpp"

#include <shaders/decal.comp.hpp>

namespace astral {

DecalPass::DecalPass(gpu::Device& device) : device_(device) {
    pipeline_ = device_.create_compute_pipeline({.shader = {shaders::decal_comp}, .debug_name = "astral.decal"});
}

DecalPass::~DecalPass() { device_.destroy(pipeline_); }

void DecalPass::add(RenderGraph& graph, const FrameInputs& inputs, GBufferTextures& gbuffer) const {
    if (inputs.scene.decals.empty()) return;
    const std::uint32_t w = inputs.extent.width;
    const std::uint32_t h = inputs.extent.height;
    const RGTexture out_albedo = graph.create("decal.albedo", {w, h, gpu::Format::RGBA8Unorm});
    const RGTexture out_normal = graph.create("decal.normal", {w, h, gpu::Format::RGBA16Float});
    const RGTexture out_emissive = graph.create("decal.emissive", {w, h, gpu::Format::RGBA16Float});
    const RGTexture out_mat_ext = graph.create("decal.material_ext", {w, h, gpu::Format::RGBA16Float});

    const GBufferTextures src = gbuffer;
    const std::uint64_t frame_address = inputs.addresses.frame;
    const gpu::PipelineHandle pipeline = pipeline_;
    graph.add_pass(
        "decals",
        [src, out_albedo, out_normal, out_emissive, out_mat_ext](PassBuilder& b) {
            read_gbuffer(b, src);
            b.write_storage(out_albedo);
            b.write_storage(out_normal);
            b.write_storage(out_emissive);
            b.write_storage(out_mat_ext);
        },
        [pipeline, frame_address, src, out_albedo, out_normal, out_emissive, out_mat_ext, w, h](PassContext& ctx) {
            const DecalPush push{frame_address,
                                 bindless_gbuffer(ctx, src),
                                 ctx.bindless(out_albedo),
                                 ctx.bindless(out_normal),
                                 ctx.bindless(out_emissive),
                                 ctx.bindless(out_mat_ext)};
            ctx.cmd.bind_pipeline(pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });
    gbuffer.albedo = out_albedo;
    gbuffer.normal = out_normal;
    gbuffer.emissive = out_emissive;
    gbuffer.material_ext = out_mat_ext;
}

} // namespace astral
