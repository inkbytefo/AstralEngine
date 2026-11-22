#pragma once
// Balls rain down on a timer. The spawner is an entity (data), the system spawns.

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace rain {

struct RainSpawner {
    float interval = 0.5f; ///< seconds between balls
    float timer = 0.0f;
};

void install_rain(astral::FeatureContext& feature);

} // namespace rain

ASTRAL_REFLECT(rain::RainSpawner, interval, timer)
