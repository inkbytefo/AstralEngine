#pragma once
// Luau-driven VFX and sound director: beacons fire PlayVfx / StopVfx and spatial AudioSource one-shots;
// smoke dispersers clear GameplaySmoke capsules on command with accompanying audio.

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

#include <cstdint>
#include <string>

namespace vfx_director {

struct Beacon {
    std::string vfx_asset = "vfx/courtyard.vfx";
    std::string tag = "courtyard";
    std::string cue_sound = "sounds/spark_burst.wav";
    float interval = 4.5f;         ///< Seconds between automatic bursts (0 = manual only).
    float timer = 0.0f;            ///< Seconds until the next automatic burst; <= 0 fires on the next step.
    float sound_volume = 0.7f;
    bool active = true;
    bool play_cue_sound = false;   ///< Also spawn a direct Luau spatial AudioSource one-shot alongside PlayVfx.
    std::uint32_t bursts_fired = 0;
    std::uint32_t stops_sent = 0;
    std::uint32_t sounds_fired = 0;
};

struct SmokeDisperser {
    astral::Vec3 from{-3.8f, 1.05f, -2.4f};
    astral::Vec3 to{-1.8f, 1.05f, -2.4f};
    float radius_m = 0.45f;
    std::uint32_t hold_steps = 20;
    std::string sound = "sounds/whoosh.wav";
    float sound_volume = 0.8f;
    std::uint32_t pulses_fired = 0;
};

/// One-shot AudioSource entity spawned by Luau; removed by Luau once playback completes.
struct SoundOneShot {
    float max_age = 3.0f;
    float age = 0.0f;
};

void install_vfx_director(astral::FeatureContext& feature);

} // namespace vfx_director

ASTRAL_REFLECT(vfx_director::Beacon, vfx_asset, tag, cue_sound, interval, timer, sound_volume, active,
               play_cue_sound, bursts_fired, stops_sent, sounds_fired)
ASTRAL_REFLECT(vfx_director::SmokeDisperser, from, to, radius_m, hold_steps, sound, sound_volume, pulses_fired)
ASTRAL_REFLECT(vfx_director::SoundOneShot, max_age, age)
