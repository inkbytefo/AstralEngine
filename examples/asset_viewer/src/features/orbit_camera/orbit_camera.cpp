#include "features/orbit_camera/orbit_camera.hpp"

#include <algorithm>
#include <cmath>

namespace orbit {

using namespace astral;

Quat camera_rotation(float yaw, float pitch) noexcept {
    return Quat::from_axis_angle({0.0f, 1.0f, 0.0f}, yaw) * Quat::from_axis_angle({1.0f, 0.0f, 0.0f}, pitch);
}

Vec3 camera_position(Vec3 target, float distance, float yaw, float pitch) noexcept {
    return target + camera_rotation(yaw, pitch) * Vec3{0.0f, 0.0f, distance};
}

OrbitCamera from_look_at(Vec3 eye, Vec3 target) noexcept {
    OrbitCamera cam{};
    cam.target = target;
    const Vec3 d = target - eye;
    const float dist = length(d);
    cam.distance = std::clamp(dist > 1e-4f ? dist : cam.distance, cam.min_distance, cam.max_distance);
    if (dist > 1e-4f) {
        cam.yaw = std::atan2(-d.x, -d.z);
        cam.pitch = std::clamp(std::atan2(d.y, std::hypot(d.x, d.z)), -1.45f, 1.45f);
    }
    return cam;
}

void install_orbit_camera(FeatureContext& feature) {
    feature.component<OrbitCamera>();
    feature.system(Phase::Update, "orbit_camera_scroll", [](Context& ctx) {
        const float scroll_y = ctx.input.scroll().y;
        if (scroll_y == 0.0f) return;
        const float factor = std::exp(-scroll_y * 0.12f);
        ctx.world.each<OrbitCamera>([&](OrbitCamera& cam) {
            if (!cam.enabled) return;
            cam.distance = std::clamp(cam.distance * factor, cam.min_distance, cam.max_distance);
        });
    });
}

} // namespace orbit
