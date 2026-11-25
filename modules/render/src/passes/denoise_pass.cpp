#include "passes/denoise_pass.hpp"

#include <shaders/denoise_atrous.comp.hpp>
#include <shaders/denoise_temporal.comp.hpp>

#include <algorithm>

namespace astral {

DenoisePass::~DenoisePass() {
    for (auto* pair : {&diffuse_, &specular_, &moments_}) {
        for (auto& t : *pair) device_.destroy(t);
    }
    destroy_pipelines();
}

bool DenoisePass::ensure_pipelines() {
    if (temporal_pipeline_.valid() && atrous_pipeline_.valid()) return true;
    temporal_pipeline_ = device_.create_compute_pipeline(
        {.shader = {shaders::denoise_temporal_comp}, .debug_name = "astral.denoise_temporal"});
    atrous_pipeline_ = device_.create_compute_pipeline(
        {.shader = {shaders::denoise_atrous_comp}, .debug_name = "astral.denoise_atrous"});
    if (!temporal_pipeline_.valid() || !atrous_pipeline_.valid()) {
        destroy_pipelines();
        return false;
    }
    return true;
}

void DenoisePass::destroy_pipelines() {
    device_.destroy(temporal_pipeline_);
    device_.destroy(atrous_pipeline_);
    temporal_pipeline_ = {};
    atrous_pipeline_ = {};
}

bool DenoisePass::resize(std::uint32_t width, std::uint32_t height) {
    if (width_ == width && height_ == height) return false;
    for (auto* pair : {&diffuse_, &specular_, &moments_}) {
        for (auto& t : *pair) device_.destroy(t);
    }
    auto make = [&](const char* name) {
        return device_.create_texture({.width = width,
                                       .height = height,
                                       .format = gpu::Format::RGBA16Float,
                                       .usage = gpu::TextureUsage::Sampled | gpu::TextureUsage::Storage,
                                       .debug_name = name});
    };
    for (int i = 0; i < 2; ++i) {
        diffuse_[i] = make("astral.history.diffuse");
        specular_[i] = make("astral.history.specular");
        moments_[i] = make("astral.history.moments");
    }
    // A failed allocation must not be committed as the new size: the same size would then never be
    // tried again and the invalid textures would stay. Size 0 makes the next frame retry.
    bool ok = true;
    for (auto* pair : {&diffuse_, &specular_, &moments_}) {
        for (auto& t : *pair) ok = ok && t.valid();
    }
    width_ = ok ? width : 0;
    height_ = ok ? height : 0;
    return true;
}

DenoisePass::TemporalOutput DenoisePass::add_temporal(RenderGraph& graph, const FrameInputs& inputs,
                                                      const GBufferTextures& gbuffer,
                                                      const RtLightingOutput& rt) const {
    const std::uint32_t w = inputs.extent.width;
    const std::uint32_t h = inputs.extent.height;
    const std::uint32_t parity = inputs.frame_index & 1u;
    const std::uint32_t prev = parity ^ 1u;

    const RGTexture hist_d = graph.import(diffuse_[parity], "history.diffuse");
    const RGTexture hist_d_prev = graph.import(diffuse_[prev], "history.diffuse.prev");
    const RGTexture hist_s = graph.import(specular_[parity], "history.specular");
    const RGTexture hist_s_prev = graph.import(specular_[prev], "history.specular.prev");
    const RGTexture moments = graph.import(moments_[parity], "history.moments");
    const RGTexture moments_prev = graph.import(moments_[prev], "history.moments.prev");

    const std::uint64_t frame_address = inputs.addresses.frame;
    const float max_history = std::max(inputs.settings.max_history, 1.0f);
    const gpu::PipelineHandle pipeline = temporal_pipeline_;
    graph.add_pass(
        "denoise.temporal",
        [gbuffer, rt, hist_d, hist_d_prev, hist_s, hist_s_prev, moments, moments_prev](PassBuilder& b) {
            read_gbuffer(b, gbuffer);
            for (RGTexture t :
                 {rt.geometry, rt.geometry_prev, rt.diffuse, rt.specular, hist_d_prev, hist_s_prev, moments_prev}) {
                b.read(t);
            }
            b.write_storage(hist_d);
            b.write_storage(hist_s);
            b.write_storage(moments);
        },
        [pipeline, frame_address, gbuffer, rt, hist_d, hist_d_prev, hist_s, hist_s_prev, moments, moments_prev,
         max_history, w, h](PassContext& ctx) {
            const TemporalPush push{frame_address,
                                    bindless_gbuffer(ctx, gbuffer),
                                    ctx.bindless(rt.geometry),
                                    ctx.bindless(rt.geometry_prev),
                                    ctx.bindless(rt.diffuse),
                                    ctx.bindless(rt.specular),
                                    ctx.bindless(hist_d_prev),
                                    ctx.bindless(hist_s_prev),
                                    ctx.bindless(hist_d),
                                    ctx.bindless(hist_s),
                                    max_history,
                                    ctx.bindless(moments_prev),
                                    ctx.bindless(moments)};
            ctx.cmd.bind_pipeline(pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });
    return {hist_d, hist_s, moments};
}

DenoiseOutput DenoisePass::add_atrous(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                                      RGTexture geometry, const TemporalOutput& temporal) const {
    const std::uint32_t w = inputs.extent.width;
    const std::uint32_t h = inputs.extent.height;
    const std::uint64_t frame_address = inputs.addresses.frame;
    const gpu::PipelineHandle pipeline = atrous_pipeline_;
    RGTexture final_d = temporal.diffuse;
    RGTexture final_s = temporal.specular;
    const std::uint32_t iterations = std::min(inputs.settings.denoise_iterations, RenderSettings::max_denoise_iterations);
    for (std::uint32_t i = 0; i < iterations; ++i) {
        const RGTexture in_d = final_d;
        const RGTexture in_s = final_s;
        const RGTexture out_d = graph.create("denoise.diffuse", {w, h, gpu::Format::RGBA16Float});
        const RGTexture out_s = graph.create("denoise.specular", {w, h, gpu::Format::RGBA16Float});
        const std::int32_t step = 1 << i;
        graph.add_pass(
            "denoise.atrous",
            [gbuffer, geometry, moments = temporal.moments, in_d, in_s, out_d, out_s](PassBuilder& b) {
                read_gbuffer(b, gbuffer);
                b.read(geometry);
                b.read(moments);
                b.read(in_d);
                b.read(in_s);
                b.write_storage(out_d);
                b.write_storage(out_s);
            },
            [pipeline, frame_address, gbuffer, geometry, moments = temporal.moments, in_d, in_s, out_d, out_s, step, w,
             h](PassContext& ctx) {
                const AtrousPush push{frame_address,
                                      bindless_gbuffer(ctx, gbuffer),
                                      ctx.bindless(geometry),
                                      ctx.bindless(in_d),
                                      ctx.bindless(in_s),
                                      ctx.bindless(out_d),
                                      ctx.bindless(out_s),
                                      step,
                                      ctx.bindless(moments)};
                ctx.cmd.bind_pipeline(pipeline);
                ctx.cmd.push_constants(&push, sizeof(push));
                ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
            });
        final_d = out_d;
        final_s = out_s;
    }
    return {final_d, final_s};
}

DenoiseOutput DenoisePass::add(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                               const RtLightingOutput& rt) const {
    const TemporalOutput temporal = add_temporal(graph, inputs, gbuffer, rt);
    return add_atrous(graph, inputs, gbuffer, rt.geometry, temporal);
}

} // namespace astral
