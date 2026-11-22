#include "passes/gbuffer_pass.hpp"

#include <shaders/gbuffer.frag.hpp>
#include <shaders/gbuffer.vert.hpp>

namespace astral {

GBufferPass::GBufferPass(gpu::Device& device) : device_(device) {
    pipeline_ = device_.create_graphics_pipeline({
        .vertex = {shaders::gbuffer_vert},
        .fragment = {shaders::gbuffer_frag},
        .color_formats = {gpu::Format::RGBA8Unorm, gpu::Format::RGBA16Float, gpu::Format::RGBA16Float,
                          gpu::Format::RG16Float},
        .depth = {.format = gpu::Format::D32Float},
        .cull = gpu::CullMode::Back,
        .debug_name = "astral.gbuffer",
    });
}

GBufferPass::~GBufferPass() { device_.destroy(pipeline_); }

GBufferTextures GBufferPass::add(RenderGraph& graph, const FrameInputs& inputs, RenderStats& stats) const {
    const std::uint32_t w = inputs.extent.width;
    const std::uint32_t h = inputs.extent.height;
    GBufferTextures out;
    out.albedo = graph.create("gbuffer.albedo", {w, h, gpu::Format::RGBA8Unorm});
    out.normal = graph.create("gbuffer.normal", {w, h, gpu::Format::RGBA16Float});
    out.emissive = graph.create("gbuffer.emissive", {w, h, gpu::Format::RGBA16Float});
    out.depth = graph.create("gbuffer.depth", {w, h, gpu::Format::D32Float});
    out.velocity = graph.create("gbuffer.velocity", {w, h, gpu::Format::RG16Float});

    graph.add_pass(
        "gbuffer",
        [out](PassBuilder& b) {
            b.color(out.albedo, gpu::LoadOp::Clear, {0, 0, 0, 1});
            b.color(out.normal, gpu::LoadOp::Clear, {0, 1, 0, 0});
            b.color(out.emissive, gpu::LoadOp::Clear, {0, 0, 0, 0});
            b.color(out.velocity, gpu::LoadOp::Clear, {0, 0, 0, 0});
            b.depth(out.depth, gpu::LoadOp::Clear, 1.0f);
        },
        [this, &inputs, &stats](PassContext& ctx) {
            const auto& order = inputs.instances.order;
            const auto& instance_shader = inputs.instances.instance_shader;
            std::uint32_t start = 0;
            std::uint32_t bound = 0xFFFFFFFFu;
            while (start < order.size()) {
                const MeshHandle handle = inputs.scene.meshes[order[start]].mesh;
                const std::uint32_t key = instance_shader[start];
                std::uint32_t end = start + 1;
                while (end < order.size() && inputs.scene.meshes[order[end]].mesh == handle &&
                       instance_shader[end] == key) {
                    ++end;
                }
                if (key != bound) {
                    const GpuShader* shader = key != 0 ? inputs.shaders.find_by_key(key) : nullptr;
                    ctx.cmd.bind_pipeline(shader ? shader->gbuffer : pipeline_);
                    bound = key;
                }
                if (const GpuMesh* mesh = inputs.meshes.get(handle)) {
                    const MeshPush push{device_.buffer_address(mesh->vertices), inputs.addresses.instances,
                                        inputs.addresses.frame, inputs.addresses.materials, start,
                                        inputs.addresses.params, inputs.scene.time, inputs.addresses.lights};
                    ctx.cmd.push_constants(&push, sizeof(push));
                    ctx.cmd.bind_index_buffer(mesh->indices, gpu::IndexType::Uint32);
                    ctx.cmd.draw_indexed(mesh->index_count, end - start);
                    ++stats.draw_calls;
                    stats.instances += end - start;
                }
                start = end;
            }
        });
    return out;
}

} // namespace astral
