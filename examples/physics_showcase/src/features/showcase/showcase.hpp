#pragma once
// The showcase's own data: named stations (the HUD shows the one the player is in), a wheel that Luau spins with torque,
// and a scanner beam that Luau casts with an ignore list and sensor hits. Rules: assets/features/showcase/showcase.luau.

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

#include <string>

namespace showcase {

/// A place in the world; the player is "in" the nearest one within `radius` (metres, on the ground plane).
struct Station {
    std::string title;
    std::string hint; ///< One line: what to look at, what to try.
    float radius = 14.0f;
};

/// Luau pushes the body with torque around `axis` until its angular speed reaches `target_speed` (rad/s).
struct Spinner {
    astral::Vec3 axis{0, 1, 0};
    float target_speed = 4.0f;
    float torque = 60.0f;
    float speed = 0.0f; ///< Measured by the rule (rad/s around `axis`).
};

/// Luau casts a ray from the entity along `direction`, ignoring the entity's own body and seeing sensors.
struct Scanner {
    astral::Vec3 direction{1, 0, 0};
    float reach = 12.0f;
    astral::Entity hit{};   ///< What the beam touches now (nothing: invalid).
    float distance = 0.0f;
    int hits = 0;           ///< Fixed steps in which the beam touched something.
};

void install_showcase(astral::FeatureContext& feature);

} // namespace showcase

ASTRAL_REFLECT(showcase::Station, title, hint, radius)
ASTRAL_REFLECT(showcase::Spinner, axis, target_speed, torque, speed)
ASTRAL_REFLECT(showcase::Scanner, direction, reach, hit, distance, hits)
