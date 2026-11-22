#pragma once
// Things that disappear after a while. Data here, the rule in Luau (assets/features/lifetime/lifetime.luau).

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace lifetime {

struct Lifetime {
    float seconds = 10.0f; ///< Time left; the entity is destroyed at zero.
};

void install_lifetime(astral::FeatureContext& feature);

} // namespace lifetime

ASTRAL_REFLECT(lifetime::Lifetime, seconds)
