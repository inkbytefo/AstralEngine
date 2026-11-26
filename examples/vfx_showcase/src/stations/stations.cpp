#include "stations/stations.hpp"

#include "features/fly_camera/fly_camera.hpp"

#include <cmath>
#include <utility>
#include <vector>

using namespace astral;

namespace vfx_showcase {

namespace {

constexpr StationInfo stations[] = {
    {"courtyard", "Avlu VFX zaman çizelgesi",
     "Luau ile tetiklenen duman, alev hacmi, kıvılcım, ışık, iz ve ses (Space patlat, X durdur, C dağıt)."},
    {"particles", "GPU parçacıklar ve hacim alevi",
     "Rüzgârda savrulan kıvılcım ve köz parçacıkları, derinlik sekmesi ve prosedürel alev hacmi."},
    {"smoke", "Oynanış dumanı ve kapsül dağıtma",
     "Küresel sis kapalıyken duvarlara takılan yerel duman. C ile koridorda görüş koridoru aç ve sesini duy."},
    {"atmosphere", "Atmosfer, gökyüzü IBL ve froxel sis",
     "Rayleigh/Mie gökyüzü, uzak silüetlerde hava perspektifi ve froxel sis (T saat, F/G/I gölge/DDGI/IBL)."},
};

std::size_t index(StationId id) noexcept { return static_cast<std::size_t>(id); }

} // namespace

const StationInfo& info(StationId id) { return stations[index(id)]; }

StationId station_from_name(std::string_view name) {
    for (std::size_t i = 0; i < std::size(stations); ++i) {
        if (name == stations[i].name) return static_cast<StationId>(i);
    }
    return StationId::Courtyard;
}

RenderSettings station_settings(StationId id) {
    RenderSettings s;
    s.sky_ibl = true;
    s.fog_self_shadows = true;
    s.fog_ddgi = true;
    switch (id) {
    case StationId::Courtyard:
    case StationId::Particles:
        s.bloom_strength = 0.06f;
        break;
    case StationId::Smoke:
    case StationId::Atmosphere:
        s.bloom_strength = 0.04f;
        break;
    }
    return s;
}

bool StationBuild::load_courtyard(float default_hour, float default_fog, bool add_colliders) {
    ctx.sun = {};
    ctx.environment = {};
    const LoadResult loaded = ctx.scenes().load_scene("courtyard.scene");
    for (const Entity e : loaded.entities) owned.push_back(e);
    if (!loaded.errors.empty()) return false;

    const float fog = overrides.baseline ? 0.0f : overrides.fog.value_or(default_fog);
    ctx.environment.fog_extinction = fog;
    if (!overrides.aerial || overrides.baseline) ctx.environment.aerial_extinction = 0.0f;

    if (auto* sky = ctx.singleton_mut<Atmosphere>()) {
        sky->hour = overrides.hour.value_or(default_hour);
        sky->hours_per_second = overrides.hours_per_second.value_or(0.0f);
        sky->enabled = !overrides.baseline;
    }

    if (add_colliders) {
        std::vector<std::pair<Entity, Vec3>> colliders;
        ctx.world.each<const PrimitiveMesh, const Transform>([&](Entity e, const PrimitiveMesh& mesh, const Transform& t) {
            if (mesh.shape == Primitive::Cube) colliders.emplace_back(e, t.scale * 0.5f);
        });
        for (const auto& [e, half] : colliders) {
            ctx.world.add(e, RigidBody{.desc = {.shape = ShapeDesc::box(half), .motion = MotionType::Static}});
        }
    }
    return true;
}

Entity StationBuild::camera(Vec3 eye, Vec3 target, float fov_degrees) {
    const Vec3 d = target - eye;
    const fly::FlyCamera flight{.speed = 4.0f,
                                .fast_speed = 12.0f,
                                .yaw = std::atan2(-d.x, -d.z),
                                .pitch = std::atan2(d.y, std::hypot(d.x, d.z))};
    const Quat rotation = Quat::from_axis_angle({0, 1, 0}, flight.yaw) * Quat::from_axis_angle({1, 0, 0}, flight.pitch);
    return create(Transform{.position = eye, .rotation = rotation},
                  Camera{.fov_y = radians(fov_degrees), .near_plane = 0.05f}, flight);
}

} // namespace vfx_showcase
