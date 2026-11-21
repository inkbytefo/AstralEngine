// Pong's rules, headless and deterministic (fixed time step, seeded serves).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "pong.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <functional>

using namespace astral;

namespace {

/// Loads the real court scene and runs `frames` frames, calling `each_frame` after every update.
struct Run final : Game {
    bool player_ai = true;
    std::function<void(Context&)> setup, each_frame;
    void on_start(Context& ctx) override {
        pong::install(ctx, player_ai);
        REQUIRE(ctx.load_scene("court.scene").errors.empty());
        if (setup) setup(ctx);
    }
    void on_update(Context& ctx) override {
        if (each_frame) each_frame(ctx);
    }
};

App make_app() {
    return App({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
}

Entity named(World& w, const char* name) {
    const Entity e = find_by_name(w, name);
    REQUIRE(e.valid());
    return e;
}

} // namespace

TEST_CASE("the court scene defines the whole game: paddles, ball, rules, score lights") {
    App app = make_app();
    Run run;
    std::size_t paddles = 0, pips = 0, balls = 0;
    pong::Court court;
    bool opponent_ai = false;
    run.setup = [&](Context& ctx) {
        paddles = ctx.world.count<pong::Paddle>();
        balls = ctx.world.count<pong::Ball>();
        pips = ctx.world.count<pong::ScorePip>();
        ctx.world.each<const pong::Court>([&](const pong::Court& c) { court = c; });
        opponent_ai = ctx.world.get<pong::Paddle>(named(ctx.world, "opponent paddle"))->ai;
    };
    app.run_frames(run, 1);
    CHECK(paddles == 2);
    CHECK(balls == 1);
    CHECK(pips == 10);
    CHECK(court.seed == 7);          // overridden in the scene
    CHECK(court.points_to_win == 5); // default
    CHECK(opponent_ai);              // from the opponent_paddle.prefab variant
}

TEST_CASE("AI against AI: the ball stays in the tunnel, rallies happen, a match ends and restarts") {
    App app = make_app();
    Run run;
    float worst_x = 0, worst_y = 0;
    bool finished = false;
    int max_points = 0;
    std::uint64_t rallies = 0;
    run.each_frame = [&](Context& ctx) {
        ctx.world.each<const pong::Ball, const Transform>([&](const pong::Ball& b, const Transform& t) {
            worst_x = std::max(worst_x, std::abs(t.position.x) + b.radius);
            worst_y = std::max(worst_y, std::abs(t.position.y) + b.radius);
        });
        const pong::Match& m = *pong::match(ctx.world);
        max_points = std::max({max_points, m.player, m.opponent});
        finished = finished || m.game_over_timer > 0;
        rallies = m.rallies;
    };
    app.run_frames(run, 60 * 240); // four minutes of play
    CHECK(worst_x <= 4.0f + 1e-4f);
    CHECK(worst_y <= 2.5f + 1e-4f);
    CHECK(rallies > 10);
    CHECK(finished);
    CHECK(max_points == 5);
}

TEST_CASE("a ball reaching the paddle comes back faster; a miss is a point for the other side and lights a pip") {
    App app = make_app();
    Run run;
    run.player_ai = false; // the player paddle stays where we put it
    int frame = 0;
    float speed_before = 0, speed_after = 0, vz_after = 0;
    int opponent_points = 0;
    Vec3 pip_emissive{}, dark_pip{};
    run.setup = [&](Context& ctx) {
        // Opponent AI off: the test drives the ball by hand.
        ctx.world.get<pong::Paddle>(named(ctx.world, "opponent paddle"))->ai = false;
    };
    run.each_frame = [&](Context& ctx) {
        World& w = ctx.world;
        const Entity ball = named(w, "ball");
        const Entity player = named(w, "player paddle");
        ++frame;
        if (frame == 2) { // straight at the paddle's centre
            w.get<Transform>(ball)->position = {0, 0, 5};
            w.get<pong::Ball>(ball)->velocity = {0, 0, 10};
            speed_before = 10;
        }
        if (frame == 40) {
            const Vec3 v = w.get<pong::Ball>(ball)->velocity;
            speed_after = length(v);
            vz_after = v.z;
            // Now move the paddle out of the way and send the ball past it.
            w.get<Transform>(player)->position = {-2.5f, -1.5f, 8.4f};
            w.get<Transform>(ball)->position = {2.5f, 1.5f, 5};
            w.get<pong::Ball>(ball)->velocity = {0, 0, 12};
        }
        if (frame == 80) {
            opponent_points = pong::match(w)->opponent;
            w.each<const pong::ScorePip, const PrimitiveMesh>([&](const pong::ScorePip& p, const PrimitiveMesh& m) {
                if (p.side == pong::Side::Opponent && p.index == 0) pip_emissive = m.material.emissive;
                if (p.side == pong::Side::Opponent && p.index == 1) dark_pip = m.material.emissive;
            });
        }
    };
    app.run_frames(run, 81);
    CHECK(vz_after < 0.0f);                               // came back
    CHECK(speed_after == doctest::Approx(speed_before * 1.06f)); // and faster
    CHECK(opponent_points == 1);
    CHECK(pip_emissive.x > 1.0f);  // first red pip lit
    CHECK(dark_pip.x < 0.1f);      // the second still dark
}

TEST_CASE("hitting off-centre bends the ball towards that side") {
    App app = make_app();
    Run run;
    run.player_ai = false;
    int frame = 0;
    Vec3 after{};
    run.setup = [&](Context& ctx) { ctx.world.get<pong::Paddle>(named(ctx.world, "opponent paddle"))->ai = false; };
    run.each_frame = [&](Context& ctx) {
        World& w = ctx.world;
        const Entity ball = named(w, "ball");
        if (++frame == 2) {
            w.get<Transform>(ball)->position = {0.9f, 0, 6}; // the paddle sits at x = 0: hit near its right edge
            w.get<pong::Ball>(ball)->velocity = {0, 0, 10};
        }
        if (frame == 30) after = w.get<pong::Ball>(ball)->velocity;
    };
    app.run_frames(run, 31);
    CHECK(after.z < 0.0f);
    CHECK(after.x > 2.0f);
    CHECK(std::abs(after.y) < 1e-4f);
}
