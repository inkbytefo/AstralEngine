#include "passes/rt_lighting_pass.hpp"

#include <algorithm>

namespace astral {

RtLightingPass::~RtLightingPass() {
    for (auto& t : geometry_) device_.destroy(t);
    for (auto& b : reservoirs_) device_.destroy(b);
    device_.destroy(reservoir_temp_);
}

bool RtLightingPass::resize(std::uint32_t width, std::uint32_t height) {
    if (width_ == width && height_ == height) return false;
    const std::uint64_t res_bytes = std::uint64_t{width} * height * sizeof(GpuReservoir);
    for (std::size_t i = 0; i < 2; ++i) {
        device_.destroy(geometry_[i]);
        geometry_[i] = device_.create_texture({.width = width,
                                               .height = height,
                                               .format = gpu::Format::RGBA16Float,
                                               .usage = gpu::TextureUsage::Sampled | gpu::TextureUsage::Storage,
                                               .debug_name = "astral.history.geometry"});
        device_.destroy(reservoirs_[i]);
        reservoirs_[i] = device_.create_buffer({.size = res_bytes, .debug_name = "astral.restir.reservoirs"});
    }
    device_.destroy(reservoir_temp_);
    reservoir_temp_ = device_.create_buffer({.size = res_bytes, .debug_name = "astral.restir.temp"});
    width_ = width;
    height_ = height;
    return true;
}

void RtLightingPass::add_restir(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                                RGTexture geometry_prev, const RtPipelines& pipelines, std::uint64_t prev_res,
                                std::uint64_t temp_res, std::uint64_t out_res) const {
    const std::uint32_t w = inputs.extent.width;
    const std::uint32_t h = inputs.extent.height;
    const FrameAddresses addr = inputs.addresses;

    graph.add_pass(
        "restir.temporal",
        [gbuffer, geometry_prev](PassBuilder& b) {
            read_gbuffer(b, gbuffer);
            b.read(geometry_prev);
            b.side_effect(); // writes reservoir_temp_ BDA buffer
        },
        [pipe = pipelines.restir_temporal, addr, gbuffer, geometry_prev, prev_res, temp_res, w, h](PassContext& ctx) {
            const RestirPush push{addr.frame,
                                  addr.tlas,
                                  addr.instances,
                                  addr.materials,
                                  addr.lights,
                                  addr.rects,
                                  addr.shapes,
                                  prev_res,
                                  temp_res,
                                  bindless_gbuffer(ctx, gbuffer),
                                  ctx.bindless(geometry_prev),
                                  0u};
            ctx.cmd.bind_pipeline(pipe);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });

    graph.add_pass(
        "restir.spatial",
        [gbuffer](PassBuilder& b) {
            read_gbuffer(b, gbuffer);
            b.side_effect(); // reads reservoir_temp_, writes reservoirs_[parity]
        },
        [pipe = pipelines.restir_spatial, addr, gbuffer, temp_res, out_res, w, h](PassContext& ctx) {
            ctx.cmd.memory_barrier(); // reservoir_temp_ written by restir.temporal
            const RestirPush push{addr.frame,
                                  addr.tlas,
                                  addr.instances,
                                  addr.materials,
                                  addr.lights,
                                  addr.rects,
                                  addr.shapes,
                                  temp_res,
                                  out_res,
                                  bindless_gbuffer(ctx, gbuffer),
                                  0u,
                                  0u};
            ctx.cmd.bind_pipeline(pipe);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });
}

RtLightingOutput RtLightingPass::add(RenderGraph& graph, const FrameInputs& inputs, const GBufferTextures& gbuffer,
                                     const DdgiOutput& ddgi, const RtPipelines& pipelines) const {
    const std::uint32_t w = inputs.extent.width;
    const std::uint32_t h = inputs.extent.height;
    const std::uint32_t parity = inputs.frame_index & 1u;
    const std::uint32_t prev = parity ^ 1u;

    const RGTexture geometry = graph.import(geometry_[parity], "history.geometry");
    const RGTexture geometry_prev = graph.import(geometry_[prev], "history.geometry.prev");
    const RGTexture noisy_d = graph.create("rt.diffuse", {w, h, gpu::Format::RGBA16Float});
    const RGTexture noisy_s = graph.create("rt.specular", {w, h, gpu::Format::RGBA16Float});

    const bool restir_active =
        inputs.settings.restir_di && (inputs.instances.lights.size() + inputs.instances.rects.size()) > 2;
    const std::uint64_t temporal_out = restir_active ? device_.buffer_address(reservoirs_[parity]) : 0;
    const std::uint64_t spatial_out = restir_active ? device_.buffer_address(reservoir_temp_) : 0;
    if (restir_active) {
        // Keep temporal history in reservoirs_[parity] (pre-spatial) to avoid spatial-temporal correlation lock-in;
        // restir.spatial writes the final shaded reservoir into reservoir_temp_.
        add_restir(graph, inputs, gbuffer, geometry_prev, pipelines, device_.buffer_address(reservoirs_[prev]),
                   temporal_out, spatial_out);
    }

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
        [pipe = pipelines.rt_lighting, addr, spatial_out, restir_active, gbuffer, ddgi, noisy_d, noisy_s, geometry,
         near_gi_radius, w, h](PassContext& ctx) {
            if (restir_active) ctx.cmd.memory_barrier(); // reservoir_temp_ written by restir.spatial
            const RtLightingPush push{addr.frame,
                                      addr.tlas,
                                      addr.instances,
                                      addr.materials,
                                      addr.lights,
                                      addr.rects,
                                      addr.shapes,
                                      spatial_out,
                                      bindless_gbuffer(ctx, gbuffer),
                                      ctx.bindless(ddgi.irradiance),
                                      ctx.bindless(ddgi.distance),
                                      ctx.bindless(noisy_d),
                                      ctx.bindless(noisy_s),
                                      ctx.bindless(geometry),
                                      near_gi_radius};
            ctx.cmd.bind_pipeline(pipe);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((w + 7) / 8, (h + 7) / 8);
        });
    return {noisy_d, noisy_s, geometry, geometry_prev};
}

} // namespace astral
