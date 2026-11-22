#pragma once
// Things that move on their own: Orbit (circle around the origin, bobbing) and Tumble (slow rotation).
// Data here, the rules in Luau (assets/features/orbits/orbits.luau).

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace orbits {

struct Orbit {
    float radius = 3.0f;
    float speed = 1.0f;
    float height = 1.0f;
    float phase = 0.0f;
};

struct Tumble {
    astral::Vec3 axis{0, 1, 0};
    float speed = 1.0f; ///< radians per second
};

void install_orbits(astral::FeatureContext& feature);

} // namespace orbits

ASTRAL_REFLECT(orbits::Orbit, radius, speed, height, phase)
ASTRAL_REFLECT(orbits::Tumble, axis, speed)
