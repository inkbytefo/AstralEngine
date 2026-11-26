// Station 1 (courtyard): full VFX timeline (smoke, flame volume, sparks, flash light, scorch decal, sound)
// driven from Luau, with courtyard physics colliders, subtle breeze, and looping ambient wind.
#include "features/vfx_director/vfx_director.hpp"
#include "stations/stations.hpp"

using namespace astral;

namespace vfx_showcase {

void build_courtyard(StationBuild& build) {
    if (!build.load_courtyard(17.0f, 0.015f, true)) {
        build.ctx.quit();
        return;
    }
    build.create(Transform{}, WindZone{.velocity = {1.2f, 0.25f, 0.3f}, .turbulence = 0.6f, .global = true});
    build.create(Transform{.position = {-2.8f, 1.05f, -2.4f}},
                 vfx_director::Beacon{.vfx_asset = "vfx/courtyard.vfx",
                                      .tag = "courtyard",
                                      .cue_sound = "sounds/spark_burst.wav",
                                      .interval = 4.5f,
                                      .timer = 0.0f,
                                      .sound_volume = 0.7f,
                                      .active = true,
                                      .play_cue_sound = false});
    build.create(Transform{}, vfx_director::SmokeDisperser{.from = {-3.8f, 1.05f, -2.4f},
                                                           .to = {-1.8f, 1.05f, -2.4f},
                                                           .radius_m = 0.45f,
                                                           .hold_steps = 20});
    build.create(Transform{.position = {0.0f, 2.0f, -2.0f}},
                 AudioSource{.sound = "sounds/courtyard_wind.wav",
                             .bus = AudioBus::Ambience,
                             .volume = 0.35f,
                             .loop = true,
                             .spatial = false,
                             .playing = true});
    build.camera({0.0f, 3.5f, 11.0f}, {-0.5f, 1.6f, -2.5f}, 58.0f);
}

} // namespace vfx_showcase
