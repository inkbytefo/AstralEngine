#pragma once
#include "features/pong_rules/types.hpp"

#include <astral/app/feature.hpp>

namespace pong {

/// A score light: lit while `side` has more than `index` points.
struct ScorePip {
    Side side = Side::Player;
    std::int32_t index = 0;
    astral::Vec3 color{1, 1, 1};
};

void install_scorelights(astral::FeatureContext& feature);

} // namespace pong

ASTRAL_REFLECT(pong::ScorePip, side, index, color)
