#include "stations/stations.hpp"

#include "features/fly_camera/fly_camera.hpp"

#include <cmath>

using namespace astral;

namespace world_showcase {

bool build_range_station(StationBuild& build) {
    if (!build.load_scene("range.scene")) return false;
    build.ctx.sun.direction = normalize(Vec3{-0.55f, -0.65f, -0.45f});

    // Look across the sandy basin and foothills toward the snow-capped mountain peak at (540, ~80, 300).
    const Vec3 eye{190.0f, 95.0f, 590.0f};
    const Vec3 target{520.0f, 45.0f, 310.0f};
    const Vec3 d = target - eye;
    const fly::FlyCamera flight{
        .speed = 50.0f,
        .fast_speed = 180.0f,
        .yaw = std::atan2(-d.x, -d.z),
        .pitch = std::atan2(d.y, std::hypot(d.x, d.z)),
        .autopilot = build.capturing ? 20.0f : 0.0f,
    };
    const Quat rotation = Quat::from_axis_angle({0, 1, 0}, flight.yaw) * Quat::from_axis_angle({1, 0, 0}, flight.pitch);
    build.create(Transform{.position = eye, .rotation = rotation},
                 Camera{.fov_y = radians(55.0f), .far_plane = 1500.0f}, StreamingSource{}, flight);
    return true;
}

bool build_hill_station(StationBuild& build) {
    if (!build.load_scene("terrain.scene")) return false;
    build.ctx.sun.direction = normalize(Vec3{-0.5f, -0.8f, -0.3f});

    const fly::FlyCamera flight{
        .speed = 30.0f,
        .fast_speed = 100.0f,
        .yaw = 0.0f,
        .pitch = radians(-43.0f),
        .autopilot = 0.0f,
    };
    const Quat rotation = Quat::from_axis_angle({1, 0, 0}, flight.pitch);
    build.create(Transform{.position = hill_camera_eye(build.far_view), .rotation = rotation},
                 Camera{.fov_y = radians(55.0f), .far_plane = 1000.0f}, flight);
    return true;
}

} // namespace world_showcase
