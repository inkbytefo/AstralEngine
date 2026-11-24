#include "passes/gbuffer_pass.hpp"

#include <shaders/gbuffer.frag.hpp>
#include <shaders/gbuffer.vert.hpp>

namespace astral {

namespace {

bool bounds_visible(const GpuMesh& mesh, const Mat4& clip_from_local, gpu::Extent2D extent) {
    // Leave a one-pixel guard band for the RT path's projection jitter.
    const float x_limit = 1.0f + 2.0f / static_cast<float>(extent.width);
    const float y_limit = 1.0f + 2.0f / static_cast<float>(extent.height);
    unsigned outside = 0x3Fu;
    for (unsigned corner = 0; corner < 8; ++corner) {
        const Vec4 clip = clip_from_local * Vec4{
            corner & 1u ? mesh.bounds_max.x : mesh.bounds_min.x,
            corner & 2u ? mesh.bounds_max.y : mesh.bounds_min.y,
            corner & 4u ? mesh.bounds_max.z : mesh.bounds_min.z, 1};
        const unsigned planes = (clip.x < -clip.w * x_limit ? 1u : 0u) |
                                (clip.x > clip.w * x_limit ? 2u : 0u) |
                                (clip.y < -clip.w * y_limit ? 4u : 0u) |
                                (clip.y > clip.w * y_limit ? 8u : 0u) |
                                (clip.z < 0.0f ? 16u : 0u) |
                                (clip.z > clip.w ? 32u : 0u);
        outside &= planes;
        if (outside == 0u) return true;
    }
    return false;
}

bool group_visible(const FrameInputs& inputs, const GpuMesh& mesh, std::uint32_t key, std::uint32_t start,
                   std::uint32_t end) {
    // Custom vertex shaders can displace the mesh beyond its imported bounds.
    if (key != 0) return true;
    for (std::uint32_t i = start; i < end; ++i) {
        const MeshInstance& instance = inputs.scene.meshes[inputs.instances.order[i]];
        if (bounds_visible(mesh, inputs.view_projection * instance.transform, inputs.extent)) return true;
    }
    return false;
}

} // namespace

GBufferPass::GBufferPass(gpu::Device& device) : device_(device) {
    pipeline_ = device_.create_graphics_pipeline({
        .vertex = {shaders::gbuffer_vert},
        .fragment = {shaders::gbuffer_frag},
        .color_formats = {gpu::Format::RGBA8Unorm, gpu::Format::RGBA16Float, gpu::Format::RGBA16Float,
                          gpu::Format::RG16Float, gpu::Format::RGBA16Float},
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
    out.material_ext = graph.create("gbuffer.material_ext", {w, h, gpu::Format::RGBA16Float});

    graph.add_pass(
        "gbuffer",
        [out](PassBuilder& b) {
            b.color(out.albedo, gpu::LoadOp::Clear, {0, 0, 0, 1});
            b.color(out.normal, gpu::LoadOp::Clear, {0, 1, 0, 0});
            b.color(out.emissive, gpu::LoadOp::Clear, {0, 0, 0, 0});
            b.color(out.velocity, gpu::LoadOp::Clear, {0, 0, 0, 0});
            b.color(out.material_ext, gpu::LoadOp::Clear, {0, 0, 0, 0});
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
                const GpuMesh* mesh = inputs.meshes.get(handle);
                if (!mesh || !group_visible(inputs, *mesh, key, start, end)) {
                    start = end;
                    continue;
                }
                if (key != bound) {
                    const GpuShader* shader = key != 0 ? inputs.shaders.find_by_key(key) : nullptr;
                    ctx.cmd.bind_pipeline(shader ? shader->gbuffer : pipeline_);
                    bound = key;
                }
                const MeshPush push{device_.buffer_address(mesh->vertices), inputs.addresses.instances,
                                    inputs.addresses.frame, inputs.addresses.materials, start,
                                    inputs.addresses.params, inputs.scene.time, 0xFFFFFFFFu, inputs.addresses.lights};
                ctx.cmd.push_constants(&push, sizeof(push));
                ctx.cmd.bind_index_buffer(mesh->indices, gpu::IndexType::Uint32);
                ctx.cmd.draw_indexed(mesh->index_count, end - start);
                ++stats.draw_calls;
                stats.instances += end - start;
                start = end;
            }
        });
    return out;
}

} // namespace astral
