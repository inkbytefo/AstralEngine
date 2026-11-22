#pragma once
// The player: WASD walks the character controller (Space jumps), the arrows turn the third-person camera.

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace player {

struct PlayerCamera {
    float yaw = 0.0f;
    float pitch = -0.35f;
    float walk_speed = 6.0f;      ///< m/s
    float turn_speed = 1.5f;      ///< radians per second
    float distance = 9.0f;        ///< behind the player
    float height = 1.6f;          ///< above the feet
};

void install_player(astral::FeatureContext& feature);

} // namespace player

ASTRAL_REFLECT(player::PlayerCamera, yaw, pitch, walk_speed, turn_speed, distance, height)
