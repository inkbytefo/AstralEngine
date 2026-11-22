#include "passes/ddgi_pass.hpp"

#include <shaders/ddgi_update.comp.hpp>

#include <algorithm>
#include <utility>

namespace astral {

namespace {

std::pair<Vec3, Vec3> fit_scene_bounds(const RenderScene& scene, const MeshStore& meshes) {
    Vec3 lo{1e30f, 1e30f, 1e30f};
    Vec3 hi{-1e30f, -1e30f, -1e30f};
    for (const MeshInstance& d : scene.meshes) {
        const GpuMesh* mesh = meshes.get(d.mesh);
        if (!mesh) continue;
        for (int i = 0; i < 8; ++i) {
            const Vec3 c{(i & 1) ? mesh->bounds_max.x : mesh->bounds_min.x,
                         (i & 2) ? mesh->bounds_max.y : mesh->bounds_min.y,
                         (i & 4) ? mesh->bounds_max.z : mesh->bounds_min.z};
            const Vec4 p = d.transform * Vec4{c.x, c.y, c.z, 1};
            lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
            hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
        }
    }
    for (const SdfShape& sh : scene.sdf) {
        const float r = std::max({sh.size.x, sh.size.y, sh.size.z}) + sh.blend;
        lo = {std::min(lo.x, sh.position.x - r), std::min(lo.y, sh.position.y - r), std::min(lo.z, sh.position.z - r)};
        hi = {std::max(hi.x, sh.position.x + r), std::max(hi.y, sh.position.y + r), std::max(hi.z, sh.position.z + r)};
    }
    if (lo.x > hi.x) lo = hi = Vec3{};
    return {lo, hi};
}

} // namespace

DdgiPass::~DdgiPass() {
    destroy_state();
    destroy_pipelines();
}

bool DdgiPass::ensure_pipelines() {
    if (update_pipeline_.valid()) return true;
    update_pipeline_ =
        device_.create_compute_pipeline({.shader = {shaders::ddgi_update_comp}, .debug_name = "astral.ddgi_update"});
    return update_pipeline_.valid();
}

void DdgiPass::destroy_pipelines() {
    device_.destroy(update_pipeline_);
    update_pipeline_ = {};
}

void DdgiPass::destroy_state() {
    for (auto& t : irradiance_) {
        device_.destroy(t);
        t = {};
    }
    for (auto& t : distance_) {
        device_.destroy(t);
        t = {};
    }
    device_.destroy(ray_buffer_);
    ray_buffer_ = {};
    nx_ = ny_ = nz_ = rays_ = 0;
    origin_ = {};
    spacing_ = {1, 1, 1};
    valid_ = false;
}

void DdgiPass::ensure(const RenderScene& scene, const MeshStore& meshes, const GiSettings& gi) {
    const std::uint32_t nx = std::max(gi.probes_x, 2u);
    const std::uint32_t ny = std::max(gi.probes_y, 2u);
    const std::uint32_t nz = std::max(gi.probes_z, 2u);
    const std::uint32_t rays = std::clamp(gi.rays_per_probe, 16u, 512u);

    Vec3 lo = gi.volume_min;
    Vec3 hi = gi.volume_max;
    if (lo.x == hi.x && lo.y == hi.y && lo.z == hi.z) {
        std::tie(lo, hi) = fit_scene_bounds(scene, meshes);
    }
    const Vec3 extent{std::max(hi.x - lo.x, 0.5f), std::max(hi.y - lo.y, 0.5f), std::max(hi.z - lo.z, 0.5f)};
    // Probes sit at cell centres so none lies exactly on a wall.
    const Vec3 spacing{extent.x / static_cast<float>(nx), extent.y / static_cast<float>(ny),
                       extent.z / static_cast<float>(nz)};
    const Vec3 origin = lo + spacing * 0.5f;

    const bool same_grid = nx_ == nx && ny_ == ny && nz_ == nz && rays_ == rays;
    const float moved = length(origin - origin_) + length(spacing - spacing_);
    if (same_grid) {
        if (moved > 0.01f * length(extent)) valid_ = false; // the volume moved: start over
        origin_ = origin;
        spacing_ = spacing;
        return;
    }
    destroy_state();
    nx_ = nx;
    ny_ = ny;
    nz_ = nz;
    rays_ = rays;
    origin_ = origin;
    spacing_ = spacing;
    auto atlas = [&](int interior, const char* name) {
        return device_.create_texture({.width = nx * ny * static_cast<std::uint32_t>(interior + 2),
                                       .height = nz * static_cast<std::uint32_t>(interior + 2),
                                       .format = gpu::Format::RGBA16Float,
                                       .usage = gpu::TextureUsage::Sampled | gpu::TextureUsage::Storage,
                                       .debug_name = name});
    };
    for (int i = 0; i < 2; ++i) {
        irradiance_[i] = atlas(8, "astral.ddgi.irradiance");
        distance_[i] = atlas(16, "astral.ddgi.distance");
    }
    ray_buffer_ =
        device_.create_buffer({.size = std::uint64_t{nx} * ny * nz * rays * 16, .debug_name = "astral.ddgi.rays"});
    valid_ = false;
}

DdgiOutput DdgiPass::add(RenderGraph& graph, const FrameInputs& inputs, gpu::PipelineHandle trace_pipeline) const {
    const std::uint32_t parity = inputs.frame_index & 1u;
    const std::uint32_t prev = parity ^ 1u;
    const RGTexture irr_prev = graph.import(irradiance_[prev], "ddgi.irradiance.prev");
    const RGTexture irr = graph.import(irradiance_[parity], "ddgi.irradiance");
    const RGTexture dist_prev = graph.import(distance_[prev], "ddgi.distance.prev");
    const RGTexture dist = graph.import(distance_[parity], "ddgi.distance");

    const FrameAddresses addr = inputs.addresses;
    const std::uint64_t rays_address = device_.buffer_address(ray_buffer_);
    const std::uint32_t probe_rays = nx_ * ny_ * nz_ * rays_;

    // Trace probe rays (lit with last frame's probes), then blend into the atlases.
    graph.add_pass(
        "ddgi.trace",
        [irr_prev, dist_prev](PassBuilder& b) {
            b.read(irr_prev);
            b.read(dist_prev);
            b.side_effect(); // writes the probe ray buffer
        },
        [trace_pipeline, addr, rays_address, irr_prev, dist_prev, probe_rays](PassContext& ctx) {
            const DdgiTracePush push{addr.frame,  addr.tlas,  addr.instances, addr.materials,
                                     addr.lights, addr.rects, addr.shapes,    rays_address,
                                     ctx.bindless(irr_prev),  ctx.bindless(dist_prev)};
            ctx.cmd.bind_pipeline(trace_pipeline);
            ctx.cmd.push_constants(&push, sizeof(push));
            ctx.cmd.dispatch((probe_rays + 63) / 64);
        });

    const gpu::PipelineHandle update_pipeline = update_pipeline_;
    for (std::uint32_t mode = 0; mode < 2; ++mode) {
        const RGTexture src = mode == 0 ? irr_prev : dist_prev;
        const RGTexture dst = mode == 0 ? irr : dist;
        const gpu::TextureDesc desc = device_.texture_desc(mode == 0 ? irradiance_[0] : distance_[0]);
        graph.add_pass(
            mode == 0 ? "ddgi.update.irradiance" : "ddgi.update.distance",
            [src, dst](PassBuilder& b) {
                b.read(src);
                b.write_storage(dst);
            },
            [update_pipeline, frame_address = addr.frame, rays_address, src, dst, mode, desc](PassContext& ctx) {
                ctx.cmd.memory_barrier(); // probe ray buffer written by ddgi.trace
                const DdgiUpdatePush push{frame_address, rays_address, ctx.bindless(src), ctx.bindless(dst), mode};
                ctx.cmd.bind_pipeline(update_pipeline);
                ctx.cmd.push_constants(&push, sizeof(push));
                ctx.cmd.dispatch((desc.width + 7) / 8, (desc.height + 7) / 8);
            });
    }
    return {irr, dist};
}

} // namespace astral
