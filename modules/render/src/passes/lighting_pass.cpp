#include "passes/lighting_pass.hpp"

#include <shaders/lighting.comp.hpp>

namespace astral {

LightingPass::LightingPass(gpu::Device& device) : device_(device) {
    pipeline_ = device_.create_compute_pipeline({.shader = {shaders::lighting_comp}, .debug_name = "astral.lighting"});
}

LightingPass::~LightingPass() { device_.destroy(pipeline_); }

RGTexture LightingPass::add(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                            RGTexture shadow_map) const {
    const std::uint32_t w = inputs.extent.width;
    const std::uint32_t h = inputs.extent.height;
    const RGTexture hdr = graph.create("hdr", {w, h, gpu::Format::RGBA16Float});
    const std::uint64_t frame_address = inputs.addresses.frame;
    const std::uint64_t light_address = inputs.addresses.lights;
    const std::uint64_t shape_address = inputs.addresses.shapes;
    const gpu::PipelineHandle pipeline = pipeline_;

    graph.add_pass(
        "lighting",
        [gbuffer, shadow_map, hdr](PassBuilder& b) {
            for (RGTexture t : {gbuffer.albedo, gbuffer.normal, gbuffer.emissive, gbuffer.depth, gbuffer.material_ext}) {
                b.read(t);
            }
            if (gbuffer.has_sdf) {
                for (RGTexture t : {gbuffer.sdf_albedo, gbuffer.sdf_normal, gbuffer.sdf_depth}) b.read(t);
            }
            if (shadow_map.valid()) b.read(shadow_map);
            b.write_storage(hdr);
        },
        [pipeline, frame_address, light_address, shape_address, gbuffer, hdr, w, h](PassContext& ctx) {
            const std::uint32_t none = ctx.bindless(gbuffer.depth); // any valid index for unused slots
            const LightingPush push{frame_address,
                                    light_address,
                                    shape_address,
                                    ctx.bindless(gbuffer.albedo),
                                    ctx.bindless(gbuffer.normal),
                                    ctx.bindless(gbuffer.emissive),
                                    ctx.bindless(gbuffer.depth),
                                    ctx.bindless(gbuffer.material_ext),
                                    gbuffer.has_sdf ? ctx.bindless(gbuffer.sdf_albedo) : none,
                                    gbuffer.has_sdf ? ctx.bindless(gbuffer.sdf_normal) : none,
                                    gbuffer.has_sdf ? ctx.bindless(gbuffer.sdf_depth) : none,
                                    ctx.bindless(hdr)};
            ctx.cmd.bind_pipeline(pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });
    return hdr;
}

} // namespace astral
