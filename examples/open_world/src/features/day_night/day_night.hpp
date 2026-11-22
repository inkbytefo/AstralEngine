#pragma once
// Day and night. The state is a component (one entity), the systems apply it: the map's "night" data layer comes on,
// the sun turns into moonlight, the sky goes dark. A HUD line shows the streaming cells and the floating origin.

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace day_night {

struct DayNight {
    bool night = false;
    int applied = -1; ///< -1 = not applied yet; otherwise the last applied night flag (runtime).
};

void install_day_night(astral::FeatureContext& feature);

/// Applies `night` to the open map's layer, the sun and the environment now.
void apply(astral::Context& ctx, bool night);

} // namespace day_night

ASTRAL_REFLECT(day_night::DayNight, night, applied)
