#include "passes/shadow_pass.hpp"

#include <shaders/shadow.vert.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace astral {

namespace {

/// Orthographic sun view fitted tightly (in light space) around every mesh's transformed
/// bounding box. Also returns the world size of one shadow texel.
Mat4 light_view_projection(const RenderScene& scene, const MeshStore& meshes, std::uint32_t map_size, bool& valid,
                           float& texel_world) {
    std::vector<Vec3> points;
    for (const MeshInstance& d : scene.meshes) {
        const GpuMesh* mesh = meshes.get(d.mesh);
        if (!mesh) continue;
        for (int i = 0; i < 8; ++i) {
            const Vec3 c{(i & 1) ? mesh->bounds_max.x : mesh->bounds_min.x,
                         (i & 2) ? mesh->bounds_max.y : mesh->bounds_min.y,
                         (i & 4) ? mesh->bounds_max.z : mesh->bounds_min.z};
            const Vec4 w = d.transform * Vec4{c.x, c.y, c.z, 1};
            points.push_back({w.x, w.y, w.z});
        }
    }
    valid = !points.empty();
    if (!valid) return Mat4::identity();
    Vec3 center{};
    for (const Vec3& p : points) center += p;
    center = center / static_cast<float>(points.size());
    const Vec3 dir = normalize(scene.sun.direction);
    const Vec3 up = std::abs(dir.y) > 0.99f ? Vec3{0, 0, 1} : Vec3{0, 1, 0};
    const Mat4 view = look_at(center - dir * 1000.0f, center, up);
    Vec3 lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
    for (const Vec3& p : points) {
        const Vec4 v = view * Vec4{p.x, p.y, p.z, 1};
        lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
        hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
    }
    const float pad = 0.05f + 0.01f * std::max(hi.x - lo.x, hi.y - lo.y);
    lo = lo - Vec3{pad, pad, pad};
    hi = hi + Vec3{pad, pad, pad};
    texel_world = std::max(hi.x - lo.x, hi.y - lo.y) / static_cast<float>(map_size);
    // View space looks down -Z: near/far are the negated z bounds. Undo orthographic()'s
    // Vulkan Y flip because the shadow map is sampled with plain UVs.
    return scale({1, -1, 1}) * orthographic(lo.x, hi.x, lo.y, hi.y, -hi.z, -lo.z) * view;
}

gpu::TextureHandle create_shadow_map(gpu::Device& device, std::uint32_t size) {
    return device.create_texture({.width = size,
                                  .height = size,
                                  .format = gpu::Format::D32Float,
                                  .usage = gpu::TextureUsage::DepthTarget | gpu::TextureUsage::Sampled,
                                  .debug_name = "astral.shadow_map"});
}

} // namespace

ShadowPass::ShadowPass(gpu::Device& device, std::uint32_t map_size) : device_(device), map_size_(map_size) {
    pipeline_ = device_.create_graphics_pipeline({
        .vertex = {shaders::shadow_vert},
        .fragment = {},
        .color_formats = {},
        .depth = {.format = gpu::Format::D32Float},
        .cull = gpu::CullMode::None, // both faces: no light leaking through thin geometry
        .debug_name = "astral.shadow",
    });
    shadow_map_ = create_shadow_map(device_, map_size_);
}

ShadowPass::~ShadowPass() {
    device_.destroy(pipeline_);
    device_.destroy(shadow_map_);
}

void ShadowPass::resize(std::uint32_t map_size) {
    if (map_size == 0 || map_size == map_size_) return;
    device_.destroy(shadow_map_);
    map_size_ = map_size;
    shadow_map_ = create_shadow_map(device_, map_size_);
}

ShadowFrame ShadowPass::prepare(const RenderScene& scene, const MeshStore& meshes) const {
    ShadowFrame out;
    bool valid = false;
    out.light_view_projection = light_view_projection(scene, meshes, map_size_, valid, out.texel_world);
    out.enabled = scene.sun.shadows && valid && !scene.meshes.empty();
    out.bindless_index = out.enabled ? device_.bindless_index(shadow_map_) : no_shadow;
    return out;
}

RGTexture ShadowPass::add(RenderGraph& graph, const FrameInputs& inputs, const ShadowFrame& shadow,
                          RenderStats& stats) const {
    if (!shadow.enabled) return {};
    const RGTexture shadow_map = graph.import(shadow_map_, "shadow_map");
    graph.add_pass(
        "shadow",
        [shadow_map](PassBuilder& b) {
            b.depth(shadow_map, gpu::LoadOp::Clear, 1.0f);
            b.side_effect(); // persistent texture read through the frame data, not the graph
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
                    gpu::PipelineHandle pipeline = pipeline_;
                    if (key != 0) {
                        const GpuShader* shader = inputs.shaders.find_by_key(key);
                        if (shader && !shader->casts_shadows) {
                            start = end;
                            continue;
                        }
                        if (shader) pipeline = shader->shadow;
                    }
                    ctx.cmd.bind_pipeline(pipeline);
                    bound = key;
                }
                if (const GpuMesh* mesh = inputs.meshes.get(handle)) {
                    const MeshPush push{device_.buffer_address(mesh->vertices), inputs.addresses.instances,
                                        inputs.addresses.frame, inputs.addresses.materials, start,
                                        inputs.addresses.params, inputs.scene.time, 0xFFFFFFFFu, inputs.addresses.lights};
                    ctx.cmd.push_constants(&push, sizeof(push));
                    ctx.cmd.bind_index_buffer(mesh->indices, gpu::IndexType::Uint32);
                    ctx.cmd.draw_indexed(mesh->index_count, end - start);
                    ++stats.shadow_draw_calls;
                }
                start = end;
            }
        });
    return shadow_map;
}

} // namespace astral
