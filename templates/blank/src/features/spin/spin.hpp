#pragma once
// Data of the "spin" feature. Exported to other features (see feature.json): components only, no logic.

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace blank {

/// Turns its entity around `axis`. The rule is Luau (assets/features/spin/spin.luau).
struct Spin {
    astral::Vec3 axis{0, 1, 0};
    float speed = 1.0f; ///< radians per second
};

void install_spin(astral::FeatureContext& feature);

} // namespace blank

ASTRAL_REFLECT(blank::Spin, axis, speed)
