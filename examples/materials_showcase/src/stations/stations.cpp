#include "stations/stations.hpp"

#include "features/fly_camera/fly_camera.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

using namespace astral;

namespace materials_showcase {

namespace {

constexpr StationInfo stations[] = {
    {"studio", "PBR stüdyo", "7 PBR modeli, car_paint.ashader ve 2 decal. P ile path tracer aç/kapat."},
    {"workflow", "Materyal iş akışı", "Ebeveyn kalıtımı, triplanar ve GLB slot ezmesi. U ile UV / texel yoğunluğu."},
    {"gallery", "Shader galerisi", ".ashader yüzey, vertex, gökyüzü (dusk_sky) ve son işlem (vignette)."},
};

std::size_t index(StationId id) { return static_cast<std::size_t>(id); }

ImageData make_checker_image(std::uint32_t size, std::uint32_t tiles) {
    std::vector<std::uint8_t> rgba(std::size_t{size} * size * 4, 255);
    const std::uint32_t cell = size / tiles;
    for (std::uint32_t y = 0; y < size; ++y) {
        for (std::uint32_t x = 0; x < size; ++x) {
            const bool light = (((x / cell) + (y / cell)) & 1u) == 0u;
            const std::uint8_t v = light ? 210 : 48;
            const std::size_t i = (std::size_t{y} * size + x) * 4;
            rgba[i] = v;
            rgba[i + 1] = v;
            rgba[i + 2] = static_cast<std::uint8_t>(light ? 216 : 56);
        }
    }
    return {.width = size, .height = size, .rgba = std::move(rgba), .srgb = true};
}

} // namespace

const StationInfo& info(StationId id) { return stations[index(id)]; }

StationId station_from_name(std::string_view name) {
    for (std::size_t i = 0; i < std::size(stations); ++i) {
        if (name == stations[i].name) return static_cast<StationId>(i);
    }
    return StationId::Studio;
}

RenderSettings station_settings(StationId id) {
    RenderSettings s;
    switch (id) {
    case StationId::Studio:
        s.path_tracer_max_bounces = 6;
        s.gi = {.probes_x = 8, .probes_y = 4, .probes_z = 6, .rays_per_probe = 64,
                .volume_min = {-7.5f, 0.0f, -4.5f}, .volume_max = {7.5f, 5.0f, 4.5f}};
        s.bloom_strength = 0.04f;
        break;
    case StationId::Workflow:
        s.path_tracer_max_bounces = 4;
        s.gi = {.probes_x = 6, .probes_y = 4, .probes_z = 4};
        s.bloom_strength = 0.04f;
        break;
    case StationId::Gallery:
        s.bloom_strength = 0.12f;
        break;
    }
    return s;
}

std::string_view uv_inspect_material(UvInspectMode mode) noexcept {
    switch (mode) {
    case UvInspectMode::Off: return {};
    case UvInspectMode::Checker: return "workflow/materials/uv_inspect.material";
    case UvInspectMode::Density: return "workflow/materials/uv_density.material";
    }
    return {};
}

MaterialHandle StationResources::material(Context& ctx, StationId station, const MaterialDesc& desc) {
    std::vector<MaterialHandle>& made = materials_[index(station)];
    if (next_ == made.size()) made.push_back(ctx.materials().create_material(desc));
    return made[next_++];
}

MeshHandle StationResources::smooth_sphere(Context& ctx) {
    if (!sphere_.valid()) sphere_ = ctx.materials().create_mesh(make_sphere(96, 64));
    return sphere_;
}

TextureHandle StationResources::checker_texture(Context& ctx) {
    if (!checker_.valid() && ctx.renderer) checker_ = ctx.renderer->create_texture(make_checker_image(256, 8));
    return checker_;
}

Entity StationBuild::camera(Vec3 eye, Vec3 target, float fov_degrees) {
    const Vec3 d = target - eye;
    const fly::FlyCamera flight{.speed = 3.5f, .fast_speed = 10.0f, .yaw = std::atan2(-d.x, -d.z),
                                .pitch = std::atan2(d.y, std::hypot(d.x, d.z))};
    const Quat rotation = Quat::from_axis_angle({0, 1, 0}, flight.yaw) * Quat::from_axis_angle({1, 0, 0}, flight.pitch);
    return create(Transform{.position = eye, .rotation = rotation},
                  Camera{.fov_y = radians(fov_degrees), .near_plane = 0.05f}, flight);
}

} // namespace materials_showcase
