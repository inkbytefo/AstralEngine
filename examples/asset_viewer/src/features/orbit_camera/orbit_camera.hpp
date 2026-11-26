#pragma once
// Orbit camera around a target point: drag with left mouse or use arrow keys to orbit,
// scroll wheel or +/- to zoom. Data lives in the component; input rules run in Luau.

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace orbit {

struct OrbitCamera {
    astral::Vec3 target{0.0f, 0.55f, 0.0f};
    float distance = 3.8f;
    float min_distance = 0.4f;
    float max_distance = 60.0f;
    float yaw = 0.0f;         ///< radians around +Y (same convention as fly::FlyCamera)
    float pitch = -0.3f;      ///< radians around +X, clamped to +-1.45
    float orbit_speed = 1.6f; ///< radians/sec for keyboard orbit
    float drag_speed = 0.006f;///< radians/pixel for mouse drag
    float zoom_speed = 2.0f;  ///< exponential zoom rate per second (keys)
    bool enabled = true;
};

[[nodiscard]] astral::Quat camera_rotation(float yaw, float pitch) noexcept;
[[nodiscard]] astral::Vec3 camera_position(astral::Vec3 target, float distance, float yaw, float pitch) noexcept;
[[nodiscard]] OrbitCamera from_look_at(astral::Vec3 eye, astral::Vec3 target) noexcept;

void install_orbit_camera(astral::FeatureContext& feature);

} // namespace orbit

ASTRAL_REFLECT(orbit::OrbitCamera, target, distance, min_distance, max_distance, yaw, pitch, orbit_speed, drag_speed,
               zoom_speed, enabled)
