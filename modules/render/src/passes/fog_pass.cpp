#include "passes/fog_pass.hpp"

#include <shaders/fog_composite.comp.hpp>
#include <shaders/fog_ddgi_reduce.comp.hpp>
#include <shaders/fog_integrate.comp.hpp>
#include <shaders/fog_volume_shadow.comp.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace astral {

namespace {

constexpr std::uint32_t brick_edge = 8, atlas_columns = 16, max_bricks = 512;
constexpr std::uint32_t max_volumes = 16, max_side = 25, max_brick_grid = (max_side + brick_edge - 1) / brick_edge;
constexpr std::uint32_t max_pages = max_brick_grid * max_brick_grid * max_brick_grid;

struct GpuSmokeVolume {
    Vec3 corner;
    float cell_size;
    float extinction;
    std::uint32_t side, first_word, first_brick, modulation = no_texture, sun_shadow = no_texture;
};
static_assert(sizeof(GpuSmokeVolume) == 40);

struct VolumeMaterial {
    gpu::PipelineHandle pipeline;
    std::uint32_t index;
    float age;
};

struct SmokeBatch {
    std::vector<GpuSmokeVolume> volumes;
    std::vector<VolumeMaterial> materials;
    std::uint32_t brick_count = 0;
    std::uint32_t budget_fallbacks = 0;
};

std::uint32_t pack_bricks(std::vector<std::uint32_t>& words, std::span<const std::uint32_t> pages, SmokeBatch& batch) {
    const auto used = static_cast<std::uint32_t>(std::ranges::count(pages, 1u));
    if (batch.brick_count + used > max_bricks) {
        ++batch.budget_fallbacks;
        return no_texture;
    }
    const auto first = static_cast<std::uint32_t>(words.size());
    for (auto page : pages) words.push_back(page ? batch.brick_count++ : no_texture);
    return first;
}

bool append_smoke_grid(const SmokeVolumeView& smoke, bool use_bricks, std::vector<std::uint32_t>& words,
                       SmokeBatch& batch, GpuSmokeVolume& volume) {
    const auto side = smoke.side;
    const auto cell_count = side * side * side;
    const auto grid = (side + brick_edge - 1) / brick_edge;
    std::array<std::uint32_t, max_pages> pages{};
    const bool procedural = smoke.occupancy.empty();
    const auto first = static_cast<std::uint32_t>(words.size());
    words.resize(first + (cell_count + 31) / 32, procedural ? 0xFFFFFFFFu : 0u);
    bool occupied = procedural;
    if (procedural) pages.fill(1);
    for (std::uint32_t i = 0; i < smoke.occupancy.size(); ++i) {
        if (!smoke.occupancy[i]) continue;
        occupied = true;
        words[first + i / 32] |= 1u << (i % 32);
        const auto bx = (i % side) / brick_edge;
        const auto by = ((i / side) % side) / brick_edge;
        const auto bz = (i / (side * side)) / brick_edge;
        pages[(bz * grid + by) * grid + bx] = 1;
    }
    volume.first_word = first;
    if (occupied && use_bricks) volume.first_brick = pack_bricks(words, std::span(pages).first(grid * grid * grid), batch);
    if (occupied && (!procedural || volume.first_brick != no_texture)) return true;
    words.resize(first);
    return false;
}

SmokeBatch prepare_smoke(gpu::Device& device, const FrameInputs& in, bool sunlight) {
    SmokeBatch batch;
    std::vector<std::uint32_t> words;
    for (const auto& smoke : in.scene.smoke) {
        if (batch.volumes.size() == max_volumes) break;
        if (smoke.side == 0 || smoke.side > max_side ||
            (!smoke.occupancy.empty() && smoke.occupancy.size() != smoke.side * smoke.side * smoke.side) ||
            !std::isfinite(smoke.extinction_per_m) || smoke.extinction_per_m <= 0 ||
            !std::isfinite(smoke.cell_size_m) || smoke.cell_size_m <= 0 ||
            !std::isfinite(smoke.centre.x) || !std::isfinite(smoke.centre.y) || !std::isfinite(smoke.centre.z)) continue;
        const float half = static_cast<float>(smoke.side) * smoke.cell_size_m * 0.5f;
        if (!std::isfinite(half)) continue;
        const auto* shader = in.materials.shader_of(smoke.material);
        const auto material = shader && shader->kind == GpuShader::Kind::Volume ? shader->forward : gpu::PipelineHandle{};
        if (smoke.occupancy.empty() && !material.valid()) continue;
        GpuSmokeVolume volume{smoke.centre - Vec3{half, half, half}, smoke.cell_size_m,
                              smoke.extinction_per_m, smoke.side, 0u, no_texture};
        if (!append_smoke_grid(smoke, sunlight || material.valid(), words, batch, volume)) continue;
        batch.volumes.push_back(volume);
        batch.materials.push_back({volume.first_brick == no_texture ? gpu::PipelineHandle{} : material,
                                   in.materials.resolve_index(smoke.material), smoke.age});
    }
    if (!batch.volumes.empty()) in.slot.fog_density.write(device, words.data(), words.size() * sizeof(std::uint32_t));
    return batch;
}

RGTexture allocate_atlas(RenderGraph& graph, std::uint32_t bricks, std::uint32_t planes, gpu::Format format,
                         std::string_view name) {
    return graph.create(name, {std::min(bricks, atlas_columns) * brick_edge,
                              (bricks + atlas_columns - 1) / atlas_columns * brick_edge * brick_edge * planes, format});
}

RGTexture record_volume_materials(RenderGraph& g, std::uint64_t frame, std::uint64_t density, const SmokeBatch& batch) {
    if (!std::ranges::any_of(batch.materials, [](const auto& m) { return m.pipeline.valid(); })) return {};
    const auto target = allocate_atlas(g, batch.brick_count, 2, gpu::Format::RGBA16Float, "volume.brick.material");
    g.add_pass("volume material atlas", [target](PassBuilder& builder) {
        builder.write_storage(target);
    }, [target, frame, density, batch](PassContext& c) {
        struct Push {
            std::uint64_t frame, density;
            Vec4 corner_cell;
            std::uint32_t side, target, material, first_word, first_brick;
            float age;
        };
        static_assert(sizeof(Push) == 56);
        for (std::size_t i = 0; i < batch.volumes.size(); ++i) {
            const auto& m = batch.materials[i];
            if (!m.pipeline.valid()) continue;
            const auto& v = batch.volumes[i];
            const Push push{frame, density, {v.corner.x, v.corner.y, v.corner.z, v.cell_size},
                            v.side, c.bindless(target), m.index, v.first_word, v.first_brick, m.age};
            c.cmd.bind_pipeline(m.pipeline);
            c.cmd.push_constants(&push, sizeof(push));
            c.cmd.dispatch((v.side + 7) / 8, (v.side * v.side + 7) / 8);
        }
    });
    return target;
}

RGTexture record_smoke_shadows(RenderGraph& g, const FrameInputs& in, gpu::PipelineHandle pipeline,
                               std::uint64_t density, const SmokeBatch& batch, RGTexture material) {
    if (batch.brick_count == 0) return {};
    const auto target = allocate_atlas(g, batch.brick_count, 1, gpu::Format::R32Float, "volume.brick.sun");
    g.add_pass("volume sunlight atlas", [material, target](PassBuilder& b) {
        if (material.valid()) b.read(material);
        b.write_storage(target);
    }, [pipeline, target, material, batch, frame = in.addresses.frame, density](PassContext& c) {
        struct Push {
            std::uint64_t frame, density;
            GpuSmokeVolume volume;
            std::uint32_t target;
        };
        static_assert(offsetof(Push, target) == 56);
        c.cmd.bind_pipeline(pipeline);
        for (std::size_t i = 0; i < batch.volumes.size(); ++i) {
            auto volume = batch.volumes[i];
            if (volume.first_brick == no_texture) continue;
            if (batch.materials[i].pipeline.valid()) volume.modulation = c.bindless(material);
            const Push push{frame, density, volume, c.bindless(target)};
            c.cmd.push_constants(&push, sizeof(push));
            c.cmd.dispatch((volume.side + 7) / 8, (volume.side * volume.side + 7) / 8);
        }
    });
    return target;
}

RGTexture record_probe_reduction(RenderGraph& g, const FrameInputs& in, gpu::PipelineHandle pipeline, DdgiOutput ddgi) {
    if (!in.settings.fog_ddgi || !pipeline.valid() || !ddgi.irradiance.valid() || !ddgi.distance.valid()) return {};
    const auto& gi = in.settings.gi;
    const auto width = std::max(gi.probes_x, 2u) * std::max(gi.probes_y, 2u);
    const auto height = std::max(gi.probes_z, 2u);
    const auto target = g.create("fog.probe_radiance", {width, height, gpu::Format::RGBA16Float});
    g.add_pass("fog probe radiance", [ddgi, target](PassBuilder& b) {
        b.read(ddgi.irradiance);
        b.write_storage(target);
    }, [pipeline, frame = in.addresses.frame, ddgi, target, width, height](PassContext& c) {
        struct Push {
            std::uint64_t frame;
            std::uint32_t irradiance, target;
        };
        const Push push{frame, c.bindless(ddgi.irradiance), c.bindless(target)};
        c.cmd.bind_pipeline(pipeline);
        c.cmd.push_constants(&push, sizeof(push));
        c.cmd.dispatch((width + 7) / 8, (height + 7) / 8);
    });
    return target;
}

} // namespace

Vec4 fog_parameters(const RenderScene& scene) noexcept {
    const auto& env = scene.environment;
    const auto positive = [](float value) { return std::isfinite(value) && value > 0; };
    const float density = positive(env.fog_extinction) && positive(env.fog_range_m) ? env.fog_extinction : 0.0f;
    const float range = positive(env.fog_range_m) ? env.fog_range_m : 100.0f;
    return {density, range, 0, 0};
}

FogPass::FogPass(gpu::Device& device) : device_(device) {
    integrate_ = device_.create_compute_pipeline({.shader = {shaders::fog_integrate_comp}, .debug_name = "astral.fog.integrate"});
    composite_ = device_.create_compute_pipeline({.shader = {shaders::fog_composite_comp}, .debug_name = "astral.fog.composite"});
    volume_shadow_ = device_.create_compute_pipeline(
        {.shader = {shaders::fog_volume_shadow_comp}, .debug_name = "astral.fog.volume_shadow"});
    probe_reduce_ = device_.create_compute_pipeline(
        {.shader = {shaders::fog_ddgi_reduce_comp}, .debug_name = "astral.fog.probe_reduce"});
}

FogPass::~FogPass() {
    device_.destroy(integrate_);
    device_.destroy(composite_);
    device_.destroy(volume_shadow_);
    device_.destroy(probe_reduce_);
}

RGTexture FogPass::integrate(RenderGraph& g, const FrameInputs& in, RGTexture shadow, RenderStats& stats, DdgiOutput ddgi) const {
    if ((fog_parameters(in.scene).x == 0 && in.scene.smoke.empty()) || !integrate_.valid() || !composite_.valid() ||
        in.settings.debug_view != DebugView::None) return {};
    const bool sunlight = volume_shadow_.valid() && in.settings.fog_self_shadows && in.scene.sun.intensity > 0 &&
                          length(in.scene.sun.direction) > 0;
    auto batch = prepare_smoke(device_, in, sunlight);
    stats.volume_bricks = batch.brick_count;
    stats.volume_brick_fallbacks = batch.budget_fallbacks;
    const auto count = static_cast<std::uint32_t>(batch.volumes.size());
    if (count == 0 && fog_parameters(in.scene).x == 0) return {};
    const auto density = count ? device_.buffer_address(in.slot.fog_density.buffer) : 0;
    const auto material = record_volume_materials(g, in.addresses.frame, density, batch);
    const auto sunlight_atlas = sunlight ? record_smoke_shadows(g, in, volume_shadow_, density, batch, material) : RGTexture{};
    const auto radiance = record_probe_reduction(g, in, probe_reduce_, ddgi);
    constexpr std::uint32_t slices = 16;
    const auto w = std::min((in.extent.width + 7) / 8, 240u);
    const auto h = std::min((in.extent.height + 7) / 8, 135u);
    const RGTexture atlas = g.create("fog.atlas", {w, h * slices, gpu::Format::RGBA16Float});
    const auto declare = [atlas, shadow, radiance, ddgi, material, sunlight_atlas](PassBuilder& b) {
        if (shadow.valid()) b.read(shadow);
        if (radiance.valid()) {
            b.read(radiance);
            b.read(ddgi.distance);
        }
        if (material.valid()) b.read(material);
        if (sunlight_atlas.valid()) b.read(sunlight_atlas);
        b.write_storage(atlas);
    };
    g.add_pass("froxel integrate", declare,
        [pipeline = integrate_, atlas, shadow, radiance, ddgi, frame = in.addresses.frame, w, h, count, density,
         this, &slot = in.slot, batch, material, sunlight_atlas](PassContext& c) mutable {
        std::uint64_t volumes = 0;
        if (count) {
            for (std::size_t i = 0; i < count; ++i) {
                if (batch.materials[i].pipeline.valid()) batch.volumes[i].modulation = c.bindless(material);
                if (sunlight_atlas.valid() && batch.volumes[i].first_brick != no_texture)
                    batch.volumes[i].sun_shadow = c.bindless(sunlight_atlas);
            }
            slot.fog_volumes.write(device_, batch.volumes.data(), count * sizeof(GpuSmokeVolume));
            volumes = device_.buffer_address(slot.fog_volumes.buffer);
        }
        struct Push {
            std::uint64_t frame, volumes, density;
            std::uint32_t atlas, shadow, w, h, count, probe_radiance, probe_distance;
        };
        const Push push{frame, volumes, density, c.bindless(atlas), shadow.valid() ? c.bindless(shadow) : no_texture,
                        w, h, count, radiance.valid() ? c.bindless(radiance) : no_texture,
                        radiance.valid() ? c.bindless(ddgi.distance) : no_texture};
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
