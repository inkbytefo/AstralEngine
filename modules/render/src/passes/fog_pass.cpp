#include "passes/fog_pass.hpp"

#include <shaders/fog_composite.comp.hpp>
#include <shaders/fog_integrate.comp.hpp>

#include <algorithm>
#include <cmath>

namespace astral {

namespace {

struct GpuSmokeVolume {
    Vec3 corner;
    float cell_size;
    float extinction;
    std::uint32_t side, first_word, modulation;
};
static_assert(sizeof(GpuSmokeVolume) == 32);

struct SmokeBatch {
    std::vector<GpuSmokeVolume> volumes;
    std::vector<RGTexture> modulation;
};

RGTexture record_volume_shader(RenderGraph& g, const FrameInputs& in, const SmokeVolumeView& smoke, Vec3 corner) {
    const auto* shader = in.materials.shader_of(smoke.material);
    if (!shader || shader->kind != GpuShader::Kind::Volume) return {};
    const auto side = smoke.side;
    const auto target = g.create("smoke.modulation", {side, side * side, gpu::Format::RGBA16Float});
    g.add_pass("volume material", [target](PassBuilder& builder) {
        builder.write_storage(target);
    }, [pipeline = shader->forward, target, side, frame = in.addresses.frame, corner, cell = smoke.cell_size_m,
        material = in.materials.resolve_index(smoke.material)](PassContext& c) {
        struct Push {
            std::uint64_t frame;
            Vec4 corner_cell;
            std::uint32_t side, target, material, padding;
        };
        const Push push{frame, {corner.x, corner.y, corner.z, cell}, side, c.bindless(target), material, 0};
        c.cmd.bind_pipeline(pipeline);
        c.cmd.push_constants(&push, sizeof(push));
        c.cmd.dispatch((side + 7) / 8, (side * side + 7) / 8);
    });
    return target;
}

SmokeBatch prepare_smoke(RenderGraph& g, gpu::Device& device, const FrameInputs& in) {
    constexpr std::size_t max_volumes = 16;
    SmokeBatch batch;
    auto& volumes = batch.volumes;
    std::vector<std::uint32_t> words;
    for (const auto& smoke : in.scene.smoke) {
        if (volumes.size() == max_volumes) break;
        if (smoke.side == 0 || smoke.side > 25 || smoke.occupancy.size() != smoke.side * smoke.side * smoke.side ||
            !std::isfinite(smoke.extinction_per_m) || smoke.extinction_per_m <= 0 ||
            !std::isfinite(smoke.cell_size_m) || smoke.cell_size_m <= 0 ||
            !std::isfinite(smoke.centre.x) || !std::isfinite(smoke.centre.y) || !std::isfinite(smoke.centre.z)) continue;
        const float half = static_cast<float>(smoke.side) * smoke.cell_size_m * 0.5f;
        if (!std::isfinite(half)) continue;
        const auto first = words.size();
        words.resize(first + (smoke.occupancy.size() + 31) / 32);
        for (std::size_t i = 0; i < smoke.occupancy.size(); ++i) {
            if (smoke.occupancy[i]) words[first + i / 32] |= 1u << (i % 32);
        }
        const Vec3 corner = smoke.centre - Vec3{half, half, half};
        volumes.push_back({corner, smoke.cell_size_m, smoke.extinction_per_m,
                           smoke.side, static_cast<std::uint32_t>(first), 0xFFFFFFFFu});
        batch.modulation.push_back(record_volume_shader(g, in, smoke, corner));
    }
    if (!volumes.empty()) in.slot.fog_density.write(device, words.data(), words.size() * sizeof(std::uint32_t));
    return batch;
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
}

FogPass::~FogPass() {
    device_.destroy(integrate_);
    device_.destroy(composite_);
}

RGTexture FogPass::integrate(RenderGraph& g, const FrameInputs& in, RGTexture shadow) const {
    if ((fog_parameters(in.scene).x == 0 && in.scene.smoke.empty()) || !integrate_.valid() || !composite_.valid() ||
        in.settings.debug_view != DebugView::None) return {};
    auto batch = prepare_smoke(g, device_, in);
    const auto count = static_cast<std::uint32_t>(batch.volumes.size());
    if (count == 0 && fog_parameters(in.scene).x == 0) return {};
    const auto density = count ? device_.buffer_address(in.slot.fog_density.buffer) : 0;
    constexpr std::uint32_t slices = 16;
    const auto w = std::min((in.extent.width + 7) / 8, 240u);
    const auto h = std::min((in.extent.height + 7) / 8, 135u);
    const RGTexture atlas = g.create("fog.atlas", {w, h * slices, gpu::Format::RGBA16Float});
    const auto textures = batch.modulation;
    g.add_pass("froxel integrate", [atlas, shadow, textures](PassBuilder& b) {
        if (shadow.valid()) b.read(shadow);
        for (auto texture : textures) if (texture.valid()) b.read(texture);
        b.write_storage(atlas);
    }, [pipeline = integrate_, atlas, shadow, frame = in.addresses.frame, w, h, count, density, this, &slot = in.slot,
        batch = std::move(batch)](PassContext& c) mutable {
        std::uint64_t volumes = 0;
        if (count) {
            for (std::size_t i = 0; i < count; ++i) {
                if (batch.modulation[i].valid()) batch.volumes[i].modulation = c.bindless(batch.modulation[i]);
            }
            slot.fog_volumes.write(device_, batch.volumes.data(), count * sizeof(GpuSmokeVolume));
            volumes = device_.buffer_address(slot.fog_volumes.buffer);
        }
        struct Push {
            std::uint64_t frame, volumes, density;
            std::uint32_t atlas, shadow, w, h, count, padding;
        };
        const Push push{frame, volumes, density, c.bindless(atlas), shadow.valid() ? c.bindless(shadow) : 0xFFFFFFFFu, w, h, count, 0};
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
