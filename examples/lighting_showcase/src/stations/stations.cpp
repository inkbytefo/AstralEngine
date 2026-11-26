#include "stations/stations.hpp"

#include "features/fly_camera/fly_camera.hpp"

#include <cmath>

using namespace astral;

namespace lighting_showcase {

namespace {

constexpr StationInfo stations[] = {
    {"room", "RGB oda", "Tek tavan paneli; renk taşması, yumuşak gölge ve küredeki oda ışın izlemeden gelir."},
    {"yard", "SDF avlusu", "Birbirine eriyen ve oyulan SDF şekilleri, güneş, dönen renkli ışıklar."},
    {"night", "Çok ışıklı gece", "36 nokta ışık. R ile ReSTIR'i kapatıp gürültüyü karşılaştır."},
};

std::size_t index(StationId id) { return static_cast<std::size_t>(id); }

} // namespace

const StationInfo& info(StationId id) { return stations[index(id)]; }

StationId station_from_name(std::string_view name) {
    for (std::size_t i = 0; i < std::size(stations); ++i) {
        if (name == stations[i].name) return static_cast<StationId>(i);
    }
    return StationId::Room;
}

RenderSettings station_settings(StationId id) {
    RenderSettings s;
    switch (id) {
    case StationId::Room:
        s.gi = {.probes_x = 8, .probes_y = 7, .probes_z = 10, .rays_per_probe = 128,
                .volume_min = {-3.0f, 0.0f, -3.5f}, .volume_max = {3.0f, 5.0f, 5.5f}};
        s.bloom_strength = 0.05f;
        break;
    case StationId::Yard: break; // the defaults: the GI volume fits the ground
    case StationId::Night:
        s.gi = {.probes_x = 10, .probes_y = 4, .probes_z = 10, .rays_per_probe = 96,
                .volume_min = {-10.0f, 0.0f, -10.0f}, .volume_max = {10.0f, 5.0f, 10.0f}};
        s.bloom_strength = 0.06f;
        break;
    }
    return s;
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

Entity StationBuild::camera(Vec3 eye, Vec3 target, float fov_degrees) {
    const Vec3 d = target - eye;
    // The fly camera owns the rotation: yaw around +y, then pitch. Start it looking at the target.
    const fly::FlyCamera flight{.speed = 3.0f, .fast_speed = 9.0f, .yaw = std::atan2(-d.x, -d.z),
                                .pitch = std::atan2(d.y, std::hypot(d.x, d.z))};
    const Quat rotation = Quat::from_axis_angle({0, 1, 0}, flight.yaw) * Quat::from_axis_angle({1, 0, 0}, flight.pitch);
    return create(Transform{.position = eye, .rotation = rotation},
                  Camera{.fov_y = radians(fov_degrees), .near_plane = 0.05f}, flight);
}

} // namespace lighting_showcase
