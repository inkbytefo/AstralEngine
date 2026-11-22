#include "passes/rt_lighting_pass.hpp"

#include <algorithm>

namespace astral {

RtLightingPass::~RtLightingPass() {
    for (auto& t : geometry_) device_.destroy(t);
}

bool RtLightingPass::resize(std::uint32_t width, std::uint32_t height) {
    if (width_ == width && height_ == height) return false;
    for (auto& t : geometry_) {
        device_.destroy(t);
        t = device_.create_texture({.width = width,
                                    .height = height,
                                    .format = gpu::Format::RGBA16Float,
                                    .usage = gpu::TextureUsage::Sampled | gpu::TextureUsage::Storage,
                                    .debug_name = "astral.history.geometry"});
    }
    width_ = width;
    height_ = height;
    return true;
}

RtLightingOutput RtLightingPass::add(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                                     const DdgiOutput& ddgi, gpu::PipelineHandle lighting_pipeline) const {
    const std::uint32_t w = inputs.extent.width;
    const std::uint32_t h = inputs.extent.height;
    const std::uint32_t parity = inputs.frame_index & 1u;
    const std::uint32_t prev = parity ^ 1u;

    const RGTexture geometry = graph.import(geometry_[parity], "history.geometry");
    const RGTexture geometry_prev = graph.import(geometry_[prev], "history.geometry.prev");
    const RGTexture noisy_d = graph.create("rt.diffuse", {w, h, gpu::Format::RGBA16Float});
    const RGTexture noisy_s = graph.create("rt.specular", {w, h, gpu::Format::RGBA16Float});

    const FrameAddresses addr = inputs.addresses;
    const float near_gi_radius = std::max(inputs.settings.near_gi_radius, 0.0f);
    graph.add_pass(
        "rt.lighting",
        [gbuffer, ddgi, noisy_d, noisy_s, geometry](PassBuilder& b) {
            read_gbuffer(b, gbuffer);
            b.read(ddgi.irradiance);
            b.read(ddgi.distance);
            b.write_storage(noisy_d);
            b.write_storage(noisy_s);
            b.write_storage(geometry);
        },
        [lighting_pipeline, addr, gbuffer, ddgi, noisy_d, noisy_s, geometry, near_gi_radius, w, h](PassContext& ctx) {
            const RtLightingPush push{addr.frame,
                                      addr.tlas,
                                      addr.instances,
                                      addr.materials,
                                      addr.lights,
                                      addr.rects,
                                      addr.shapes,
                                      bindless_gbuffer(ctx, gbuffer),
                                      ctx.bindless(ddgi.irradiance),
                                      ctx.bindless(ddgi.distance),
                                      ctx.bindless(noisy_d),
                                      ctx.bindless(noisy_s),
                                      ctx.bindless(geometry),
                                      near_gi_radius};
            ctx.cmd.bind_pipeline(lighting_pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });
    return {noisy_d, noisy_s, geometry, geometry_prev};
}

} // namespace astral
