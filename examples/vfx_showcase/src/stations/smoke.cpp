// Station 3 (smoke): local GameplaySmoke flooding the courtyard corridor with global froxel fog turned off,
// blocked by physics walls and cleared along a capsule on command (C) from Luau with spatial whoosh audio.
#include "features/vfx_director/vfx_director.hpp"
#include "stations/stations.hpp"

using namespace astral;

namespace vfx_showcase {

void build_smoke(StationBuild& build) {
    if (!build.load_courtyard(16.8f, 0.0f, true)) {
        build.ctx.quit();
        return;
    }
    build.create(Transform{.position = {-2.8f, 1.05f, -2.4f}},
                 GameplaySmoke({.radius_cells = 5,
                                .cell_size_m = 0.4f,
                                .extinction_per_m = 1.7f,
                                .spread_interval_steps = 1,
                                .spread_steps = 480,
                                .fade_steps = 120}));
    build.create(Transform{}, vfx_director::SmokeDisperser{.from = {-3.8f, 1.05f, -2.4f},
                                                           .to = {-1.8f, 1.05f, -2.4f},
                                                           .radius_m = 0.45f,
                                                           .hold_steps = 20,
                                                           .sound = "sounds/whoosh.wav",
                                                           .sound_volume = 0.85f});
    build.camera({-2.2f, 2.6f, 5.8f}, {-3.0f, 1.2f, -3.2f}, 56.0f);
}

} // namespace vfx_showcase
