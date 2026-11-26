#include "stations/stations.hpp"

#include "features/fly_camera/fly_camera.hpp"

using namespace astral;

namespace world_showcase {

bool build_world_station(StationBuild& build) {
    if (!build.ctx.maps().open_world("maps/world.world")) return false;
    build.ctx.sun.direction = normalize(Vec3{-0.4f, -0.55f, -0.35f});

    // Place the camera at world (1700, 40, 1700 + nudge) relative to the current floating origin.
    // On a fresh run origin is (0, 0, 0), so the camera starts 2.4 km out and triggers the 2 km shift.
    const WorldPosition origin = build.ctx.maps().world_origin();
    const Vec3 local_start{
        static_cast<float>(1700.0 - origin.x),
        55.0f,
        static_cast<float>(1700.0 + static_cast<double>(build.nudge) - origin.z),
    };
    const fly::FlyCamera flight{
        .speed = 80.0f,
        .fast_speed = 400.0f,
        .yaw = radians(-135.0f),
        .pitch = radians(-10.0f),
        .autopilot = build.capturing ? 60.0f : 0.0f,
    };
    const Quat rotation = Quat::from_axis_angle({0, 1, 0}, flight.yaw) * Quat::from_axis_angle({1, 0, 0}, flight.pitch);
    build.create(Transform{.position = local_start, .rotation = rotation}, Camera{.far_plane = 2500.0f},
                 StreamingSource{}, flight);
    return true;
}

} // namespace world_showcase
