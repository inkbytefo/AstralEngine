#include "passes/sky_pass.hpp"

#include <shaders/sky_ibl_filter.comp.hpp>

#include <bit>

namespace astral {

SkyPass::SkyPass(gpu::Device& device) : device_(device) {
    filter_ = device_.create_compute_pipeline({.shader = {shaders::sky_ibl_filter_comp}, .debug_name = "astral.sky_ibl_filter"});
}

SkyPass::~SkyPass() { device_.destroy(filter_); }

void SkyPass::bake_environment(RenderGraph& graph, const FrameInputs& in) const {
    if (!filter_.valid() || !in.settings.sky_ibl || in.settings.debug_view != DebugView::None) return;
    const auto* shader = in.materials.shader_of(in.scene.sky);
    if (!shader || !shader->sky_capture.valid()) return;

    constexpr std::uint32_t width = 32, height = 16, levels = 7;
    const auto radiance = graph.create("sky.radiance", {width * 2, height * 2, gpu::Format::RGBA16Float});
    const auto filtered = graph.create("sky.filtered", {width, height * levels, gpu::Format::RGBA16Float});
    graph.add_pass("sky environment capture", [radiance](PassBuilder& b) {
        b.color(radiance);
    }, [pipeline = shader->sky_capture, frame = in.addresses.frame, materials = in.addresses.materials,
         index = in.materials.resolve_index(in.scene.sky)](PassContext& c) {
        const ScreenPush push{frame, materials, index, no_texture};
        c.cmd.bind_pipeline(pipeline);
        c.cmd.push_constants(&push, sizeof(push));
        c.cmd.draw(3);
    });
    graph.add_pass("sky IBL filter", [radiance, filtered](PassBuilder& b) {
        b.read(radiance);
        b.write_storage(filtered);
    }, [pipeline = filter_, radiance, filtered](PassContext& c) {
        const std::uint32_t push[]{c.bindless(radiance), c.bindless(filtered), width, height};
        c.cmd.bind_pipeline(pipeline);
        c.cmd.push_constants(push, sizeof(push));
        c.cmd.dispatch((width + 7) / 8, (height * levels + 7) / 8);
    });
    // Publish both sampled textures before the frame consumers (lighting, DDGI, and RT misses).
    // The current ring slot is exclusive until this command list is submitted.
    graph.add_pass("sky IBL bind", [radiance, filtered](PassBuilder& b) {
        b.read(radiance);
        b.read(filtered);
        b.side_effect();
    }, [this, frame_buffer = in.slot.frame.buffer, radiance, filtered](PassContext& c) {
        const std::uint32_t ids[]{c.bindless(radiance), c.bindless(filtered), 1u, 0u};
        if (ids[0] != no_texture && ids[1] != no_texture)
            device_.write_buffer(frame_buffer, ids, sizeof(ids), offsetof(GpuFrame, atmosphere) + offsetof(GpuAtmosphere, ibl));
    });
}

std::uint64_t SkyPass::environment_key(const RenderScene& scene, const RenderSettings& settings,
                                       const MaterialStore& materials) const {
    if (!filter_.valid() || !settings.sky_ibl || settings.debug_view != DebugView::None) return 0;
    const auto* shader = materials.shader_of(scene.sky);
    const auto* material = materials.get(scene.sky);
    if (!shader || !shader->sky_capture.valid() || !material) return 0;
    std::uint64_t key = 14695981039346656037ull;
    const auto hash = [&key](std::uint32_t value) { key = (key ^ value) * 1099511628211ull; };
    hash(material->shader.index);
    hash(material->shader.generation);
    for (const float v : {scene.sun.direction.x, scene.sun.direction.y, scene.sun.direction.z,
                          scene.sun.color.x, scene.sun.color.y, scene.sun.color.z, scene.sun.intensity}) {
        hash(std::bit_cast<std::uint32_t>(v));
    }
    for (const float v : material->params) hash(std::bit_cast<std::uint32_t>(v));
    for (const auto& texture : material->textures) {
        hash(texture.offset);
        hash(texture.texture.index);
        hash(texture.texture.generation);
    }
    if (shader->sky_uses_time) hash(std::bit_cast<std::uint32_t>(scene.time));
    return key;
}

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
