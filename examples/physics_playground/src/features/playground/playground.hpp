#pragma once
// Data of the playground: a jump pad (trigger), a crate dropper. The rules are Luau (assets/features/playground/playground.luau)
// and one small C++ system (the dropper spawns bodies, which needs the physics description).

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace playground {

/// A sensor volume that throws what enters it upwards (the rule: playground.luau).
struct JumpPad {
    float speed = 12.0f; ///< Added vertical velocity (m/s).
};

/// Drops a light crate every `interval` seconds at its entity's position.
struct Dropper {
    float interval = 4.0f;
    float timer = 0.0f;
};

void install_playground(astral::FeatureContext& feature);

} // namespace playground

ASTRAL_REFLECT(playground::JumpPad, speed)
ASTRAL_REFLECT(playground::Dropper, interval, timer)
