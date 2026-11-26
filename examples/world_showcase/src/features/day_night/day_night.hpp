#pragma once
// Day and night feature. State lives in the DayNight component; the system flips the open map's "night" data layer,
// the sun and the environment sky when the toggle_night action (N) fires.

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace day_night {

struct DayNight {
    bool night = false;
    int applied = -1; ///< -1 = not applied yet; otherwise the last applied night flag (runtime).
};

void install_day_night(astral::FeatureContext& feature);

/// Applies `night` to the open map's "night" layer (if a map is open), the sun and the environment now.
void apply(astral::Context& ctx, bool night);

} // namespace day_night

ASTRAL_REFLECT(day_night::DayNight, night, applied)
