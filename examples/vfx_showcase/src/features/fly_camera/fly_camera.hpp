#pragma once
// A free-flying camera: WASD, Q/E down/up, Shift fast, hold the right mouse button to look.
// Data here, the rule in Luau (assets/features/fly_camera/fly_camera.luau). Put FlyCamera on the camera entity.

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace fly {

struct FlyCamera {
    float speed = 8.0f;       ///< m/s
    float fast_speed = 24.0f; ///< m/s while the "fast" action is held
    float yaw = 0.0f;         ///< radians, turned around +y
    float pitch = 0.0f;       ///< radians, clamped to +-1.4
    float look_speed = 0.003f;///< radians per pixel of mouse movement
    bool look_needs_button = true; ///< false: the mouse always looks
    float autopilot = 0.0f;   ///< > 0: flies straight ahead at this speed (captures, demos); input is ignored
};

void install_fly_camera(astral::FeatureContext& feature);

} // namespace fly

ASTRAL_REFLECT(fly::FlyCamera, speed, fast_speed, yaw, pitch, look_speed, look_needs_button, autopilot)
