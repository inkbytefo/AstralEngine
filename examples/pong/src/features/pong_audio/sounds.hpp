#pragma once
#include <astral/app/feature.hpp>
#include <astral/astral.hpp>

#include <string>

namespace pong {

/// What the match sounds like (asset paths; empty = silent). On the court entity.
struct Sounds {
    std::string paddle = "sounds/paddle.wav";
    std::string wall = "sounds/wall.wav";
    std::string serve = "sounds/serve.wav";
    std::string point_won = "sounds/point_won.wav";   ///< You scored.
    std::string point_lost = "sounds/point_lost.wav";
    std::string match_won = "sounds/match_won.wav";
    std::string match_lost = "sounds/match_lost.wav";
    float volume = 0.8f;
};

void install_audio(astral::FeatureContext& feature);

} // namespace pong

ASTRAL_REFLECT(pong::Sounds, paddle, wall, serve, point_won, point_lost, match_won, match_lost, volume)
