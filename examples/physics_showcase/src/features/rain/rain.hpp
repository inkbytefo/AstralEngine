#pragma once
// Balls rain down on a timer around the spawner entity's position. The spawner is data, the system spawns.

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace rain {

struct RainSpawner {
    float interval = 0.5f; ///< seconds between balls
    float timer = 0.0f;
    float radius = 3.0f;   ///< balls fall on a circle of this radius around the spawner
};

void install_rain(astral::FeatureContext& feature);

} // namespace rain

ASTRAL_REFLECT(rain::RainSpawner, interval, timer, radius)
