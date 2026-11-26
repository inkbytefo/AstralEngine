#pragma once
// Data of the match: components and events. The rules that use them are Luau (rules.luau).
// Exported to other features (see feature.json).

#include <astral/astral.hpp>

#include <cstdint>

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
    bool coming = false;          ///< AI runtime: the ball is coming its way.
    astral::Vec2 plan_offset{};   ///< AI runtime: where on the paddle it aims to hit, and its misjudgement.
};

struct Ball {
    float radius = 0.3f;
    float serve_speed = 9.0f;
    float max_speed = 26.0f;
    float speedup = 1.06f;        ///< Speed factor per paddle hit.
    float spin = 0.6f;            ///< How much the hit position bends the ball.
    astral::Vec3 velocity{};      ///< Runtime (zero while waiting for a serve).
};

/// Follows the ball's depth (the ring that shows how far away it is).
struct DepthMarker {};

/// Match state. On the court entity.
struct Match {
    std::int32_t player = 0;
    std::int32_t opponent = 0;
    float serve_timer = 0.0f;
    Side serve_to = Side::Player;
    float game_over_timer = 0.0f; ///< > 0: the match ended, a new one starts when it runs out.
    std::uint32_t rallies = 0;    ///< Paddle hits so far.
    std::uint32_t serve_rng = 1;  ///< Random state of the serves (Luau draws from it: same seed, same match).
    std::uint32_t ai_rng = 1;     ///< Random state of the AI's aim.
    bool seeded = false;
};

/// Sent when a side scores.
struct Scored {
    Side by = Side::Player;
};

/// Sent when the ball is served or bounces (fixed step); sounds and effects listen to it.
struct Bounced {
    enum class On : std::uint8_t { Serve, Wall, Paddle } on = On::Serve;
    astral::Vec3 position{};
    float speed = 0.0f;           ///< m/s after the bounce.
};

/// Snapshot of a match saved to / loaded from an engine save slot (astral.save / astral.load).
/// Kept on its own entity so loading only replaces the snapshot, leaving the court scene intact.
struct MatchSave {
    std::int32_t player = 0;
    std::int32_t opponent = 0;
    float serve_timer = 0.0f;
    Side serve_to = Side::Player;
    float game_over_timer = 0.0f;
    std::uint32_t rallies = 0;
    std::uint32_t serve_rng = 1;
    std::uint32_t ai_rng = 1;
    bool seeded = false;
    astral::Vec3 ball_position{};
    astral::Vec3 ball_velocity{};
    astral::Vec3 player_position{0.0f, 0.0f, 8.4f};
    bool player_coming = false;
    astral::Vec2 player_plan_offset{};
    astral::Vec3 opponent_position{0.0f, 0.0f, -8.4f};
    bool opponent_coming = false;
    astral::Vec2 opponent_plan_offset{};
};

/// Sent when a match is saved, loaded, or when no save slot exists yet (HUD feedback).
struct SaveNotice {
    enum class Kind : std::uint8_t { Saved, Loaded, Missing } kind = Kind::Saved;
};

} // namespace pong

ASTRAL_REFLECT_ENUM(pong::Side, Player, Opponent)
ASTRAL_REFLECT_ENUM(pong::Bounced::On, Serve, Wall, Paddle)
ASTRAL_REFLECT_ENUM(pong::SaveNotice::Kind, Saved, Loaded, Missing)
ASTRAL_REFLECT(pong::Court, half_extents, paddle_z, points_to_win, serve_delay, seed)
ASTRAL_REFLECT(pong::Paddle, side, ai, half_size, speed, reaction, accuracy, coming, plan_offset)
ASTRAL_REFLECT(pong::Ball, radius, serve_speed, max_speed, speedup, spin, velocity)
ASTRAL_REFLECT_EMPTY(pong::DepthMarker)
ASTRAL_REFLECT(pong::Match, player, opponent, serve_timer, serve_to, game_over_timer, rallies, serve_rng, ai_rng, seeded)
ASTRAL_REFLECT(pong::MatchSave, player, opponent, serve_timer, serve_to, game_over_timer, rallies, serve_rng, ai_rng,
               seeded, ball_position, ball_velocity, player_position, player_coming, player_plan_offset,
               opponent_position, opponent_coming, opponent_plan_offset)
ASTRAL_REFLECT(pong::Scored, by)
ASTRAL_REFLECT(pong::Bounced, on, position, speed)
ASTRAL_REFLECT(pong::SaveNotice, kind)
