#pragma once
// 3D Pong ("Curveball"): a ball in a tunnel, your paddle at the near end, the AI's at the far end.
// The court, paddles and rules live in assets/*.scene / *.prefab; this file is only behaviour.

#include <astral/astral.hpp>

#include <cstdint>
#include <random>

namespace pong {

enum class Side : std::uint8_t { Player, Opponent };

/// Rules and size of the tunnel (one per scene).
struct Court {
    astral::Vec3 half_extents{4.0f, 2.5f, 9.0f}; ///< Inner half size; the paddles play at z = +-paddle_z.
    float paddle_z = 8.4f;
    std::int32_t points_to_win = 5;
    float serve_delay = 1.0f;     ///< Seconds before a serve.
    std::uint32_t seed = 1;       ///< Serve angles: same seed, same match.
};

struct Paddle {
    Side side = Side::Player;
    bool ai = false;
    astral::Vec2 half_size{1.2f, 0.8f};
    float speed = 9.0f;           ///< m/s
    float reaction = 1.0f;        ///< AI: fraction of `speed` it moves at (lower = easier).
    float accuracy = 0.9f;        ///< AI: 1 = reads the ball perfectly; lower misjudges fast balls more.
};

struct Ball {
    float radius = 0.3f;
    float serve_speed = 9.0f;
    float max_speed = 26.0f;
    float speedup = 1.06f;        ///< Speed factor per paddle hit.
    float spin = 0.6f;            ///< How much the hit position bends the ball.
    astral::Vec3 velocity{};      ///< Runtime (zero while waiting for a serve).
};

/// A score light: lit while `side` has more than `index` points.
struct ScorePip {
    Side side = Side::Player;
    std::int32_t index = 0;
    astral::Vec3 color{1, 1, 1};
};

/// Follows the ball's depth (the ring that shows how far away it is).
struct DepthMarker {};

/// Sent when a side scores.
struct Scored {
    Side by;
};

/// Match state (a world resource).
struct Match {
    std::int32_t player = 0;
    std::int32_t opponent = 0;
    float serve_timer = 0.0f;
    Side serve_to = Side::Player;
    float game_over_timer = 0.0f; ///< > 0: the match ended, a new one starts when it runs out.
    std::uint64_t rallies = 0;    ///< Paddle hits so far.
    std::minstd_rand rng{1};
    bool seeded = false;
};

/// Registers the components (so scene files can use them) and the systems. Call in on_start,
/// before loading the court scene. `player_ai`: the AI plays your paddle too (demo / capture).
void install(astral::Context& ctx, bool player_ai = false);

/// Current match state (null before install).
const Match* match(astral::World& world);

} // namespace pong

ASTRAL_REFLECT_ENUM(pong::Side, Player, Opponent)
ASTRAL_REFLECT(pong::Court, half_extents, paddle_z, points_to_win, serve_delay, seed)
ASTRAL_REFLECT(pong::Paddle, side, ai, half_size, speed, reaction, accuracy)
ASTRAL_REFLECT(pong::Ball, radius, serve_speed, max_speed, speedup, spin)
ASTRAL_REFLECT(pong::ScorePip, side, index, color)
ASTRAL_REFLECT_EMPTY(pong::DepthMarker)
