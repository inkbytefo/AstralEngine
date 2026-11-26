#pragma once
// Counts frames and time. Data in C++, the rule in Luau (assets/features/tally/tally.luau).

#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

namespace tally {

struct Tally {
    std::int32_t frames = 0;
    float seconds = 0.0f;
    bool paused = false;
    std::int32_t resets = 0;
    std::int32_t boosted_frames = 0;
};

void install_tally(astral::FeatureContext& feature);

} // namespace tally

ASTRAL_REFLECT(tally::Tally, frames, seconds, paused, resets, boosted_frames)
