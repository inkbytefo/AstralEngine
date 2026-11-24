#include "passes/fog_pass.hpp"

#include <shaders/fog_composite.comp.hpp>
#include <shaders/fog_integrate.comp.hpp>

#include <algorithm>
#include <cmath>

namespace astral {

Vec4 fog_parameters(const Environment& env) noexcept {
    if (!std::isfinite(env.fog_extinction) || !std::isfinite(env.fog_range_m) ||
        env.fog_extinction <= 0 || env.fog_range_m <= 0) return {};
    return {env.fog_extinction, env.fog_range_m, 0, 0};
}

FogPass::FogPass(gpu::Device& device) : device_(device) {
    integrate_ = device_.create_compute_pipeline({.shader = {shaders::fog_integrate_comp}, .debug_name = "astral.fog.integrate"});
    composite_ = device_.create_compute_pipeline({.shader = {shaders::fog_composite_comp}, .debug_name = "astral.fog.composite"});
}

FogPass::~FogPass() {
    device_.destroy(integrate_);
    device_.destroy(composite_);
}

RGTexture FogPass::integrate(RenderGraph& g, const FrameInputs& in, RGTexture shadow) const {
    if (fog_parameters(in.scene.environment).x == 0 || !integrate_.valid() || !composite_.valid() ||
        in.settings.debug_view != DebugView::None) return {};
    constexpr std::uint32_t slices = 16;
    const auto w = std::min((in.extent.width + 7) / 8, 240u);
    const auto h = std::min((in.extent.height + 7) / 8, 135u);
    const RGTexture atlas = g.create("fog.atlas", {w, h * slices, gpu::Format::RGBA16Float});
    g.add_pass("froxel integrate", [atlas, shadow](PassBuilder& b) {
        if (shadow.valid()) b.read(shadow);
        b.write_storage(atlas);
    }, [pipeline = integrate_, atlas, shadow, frame = in.addresses.frame, w, h](PassContext& c) {
        struct Push {
            std::uint64_t frame;
            std::uint32_t atlas, shadow, w, h;
        };
        const Push push{frame, c.bindless(atlas), shadow.valid() ? c.bindless(shadow) : 0xFFFFFFFFu, w, h};
        c.cmd.bind_pipeline(pipeline);
        c.cmd.push_constants(&push, sizeof(push));
        c.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
    });
    return atlas;
}

RGTexture FogPass::composite(RenderGraph& g, const FrameInputs& in, RGTexture hdr,
                             const GBufferTextures& gb, RGTexture atlas) const {
    if (!atlas.valid()) return hdr;
    const auto [w, h] = in.extent;
    const RGTexture result = g.create("fog.hdr", {w, h, gpu::Format::RGBA16Float});
    g.add_pass("froxel composite", [hdr, gb, atlas, result](PassBuilder& b) {
        b.read(hdr);
        b.read(gb.depth);
        if (gb.has_sdf) b.read(gb.sdf_depth);
        b.read(atlas);
        b.write_storage(result);
    }, [pipeline = composite_, hdr, gb, atlas, result, frame = in.addresses.frame, w, h](PassContext& c) {
        struct Push {
            std::uint64_t frame;
            std::uint32_t hdr, depth, sdf_depth, atlas, result, padding;
        };
        const Push push{frame, c.bindless(hdr), c.bindless(gb.depth), gb.has_sdf ? c.bindless(gb.sdf_depth) : 0xFFFFFFFFu,
                        c.bindless(atlas), c.bindless(result), 0};
        c.cmd.bind_pipeline(pipeline);
        c.cmd.push_constants(&push, sizeof(push));
        c.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
    });
    return result;
}

} // namespace astral
