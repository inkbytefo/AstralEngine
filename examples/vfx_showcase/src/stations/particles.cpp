// Station 2 (particles): two Luau-driven braziers showcasing GPU-simulated additive sparks with depth bounce,
// blend_mix lit embers, procedural volumetric flame, and a directional wind zone bending particle drift.
#include "features/vfx_director/vfx_director.hpp"
#include "stations/stations.hpp"

using namespace astral;

namespace vfx_showcase {

void build_particles(StationBuild& build) {
    if (!build.load_courtyard(17.7f, 0.008f, true)) {
        build.ctx.quit();
        return;
    }
    build.create(Transform{},
                 WindZone{.velocity = {4.2f, 0.7f, 0.4f}, .gust_strength = 1.2f, .turbulence = 1.4f, .global = true});
    build.create(Transform{.position = {-0.8f, 0.35f, -1.6f}},
                 vfx_director::Beacon{.vfx_asset = "vfx/sparks_fountain.vfx",
                                      .tag = "particles_sparks",
                                      .cue_sound = "sounds/spark_burst.wav",
                                      .interval = 2.4f,
                                      .timer = 0.0f,
                                      .sound_volume = 0.75f,
                                      .active = true,
                                      .play_cue_sound = false});
    build.create(Transform{.position = {1.7f, 0.35f, -2.2f}},
                 vfx_director::Beacon{.vfx_asset = "vfx/flame_pillar.vfx",
                                      .tag = "particles_flame",
                                      .cue_sound = "sounds/ignition.wav",
                                      .interval = 2.6f,
                                      .timer = 0.0f,
                                      .sound_volume = 0.65f,
                                      .active = true,
                                      .play_cue_sound = false});
    build.camera({0.5f, 2.3f, 5.2f}, {0.4f, 1.1f, -2.0f}, 56.0f);
}

} // namespace vfx_showcase
