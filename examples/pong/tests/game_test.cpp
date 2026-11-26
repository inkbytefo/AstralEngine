// Pong's rules, headless and deterministic (fixed time step, seeded serves).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <vector>

using namespace astral;

namespace {

/// Loads the real court scene and runs `frames` frames, calling `each_frame` after every update.
struct Run final : Game {
    bool player_ai = true;
    std::function<void(Context&)> setup, each_frame;
    void on_start(Context& ctx) override {
        REQUIRE(pong::install(ctx));
        REQUIRE(ctx.scenes().load_scene("court.scene").errors.empty());
        pong::set_player_ai(ctx.world, player_ai);
        if (setup) setup(ctx);
    }
    void on_update(Context& ctx) override {
        if (each_frame) each_frame(ctx);
    }
};

struct TempSaveDir {
    std::filesystem::path path;
    explicit TempSaveDir(const char* tag) : path(std::filesystem::temp_directory_path() / tag) {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }
    ~TempSaveDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

App make_app(std::string_view save_folder = "saves", std::span<const InputScriptEvent> script = {}) {
    return App({.headless = true,
                .fixed_timestep = 1.0f / 60.0f,
                .asset_root = ASTRAL_GAME_ASSETS,
                .hot_reload = false,
                .save_folder = save_folder,
                .input_script = script});
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

TEST_CASE("the match is heard: serves and bounces are events, the court sounds load and play") {
    App app = make_app();
    Run run;
    int serves = 0, walls = 0, paddles = 0;
    std::uint32_t most_voices = 0;
    bool sounds_load = false;
    EventReader<pong::Bounced> reader;
    run.setup = [&](Context& ctx) {
        const pong::Sounds s;
        sounds_load = ctx.audio_service().sound(s.paddle).valid() && ctx.audio_service().sound(s.wall).valid() &&
                      ctx.audio_service().sound(s.serve).valid() && ctx.audio_service().sound(s.point_won).valid() &&
                      ctx.audio_service().sound(s.match_lost).valid();
    };
    run.each_frame = [&](Context& ctx) {
        reader.read(ctx.world, [&](const pong::Bounced& b) {
            (b.on == pong::Bounced::On::Serve ? serves : b.on == pong::Bounced::On::Wall ? walls : paddles) += 1;
        });
        most_voices = std::max(most_voices, ctx.audio->voice_count());
        std::vector<float> mix(800 * ctx.audio->channels()); // pull audio like a device would
        ctx.audio->read(mix);
    };
    app.run_frames(run, 60 * 20);
    CHECK(sounds_load);
    CHECK(serves >= 1);
    CHECK(walls >= 1);
    CHECK(paddles >= 2);
    CHECK(most_voices >= 2); // the court hum plus effects
}

namespace {

int count_children_of(World& w, Entity parent) {
    int count = 0;
    w.each<const Parent>([&](const Parent& p) {
        if (p.entity == parent) ++count;
    });
    return count;
}

void seed_custom_match(World& w) {
    pong::Match& m = *w.get<pong::Match>(named(w, "court"));
    m.player = 3;
    m.opponent = 2;
    m.rallies = 12;
    m.serve_rng = 12345;
    m.ai_rng = 67890;
    m.serve_timer = 0.0f;
    m.seeded = true;
    const Entity ball = named(w, "ball");
    w.get<Transform>(ball)->position = {1.1f, -0.4f, 1.5f};
    w.get<pong::Ball>(ball)->velocity = {2.0f, -1.0f, -6.0f};
    w.get<Transform>(named(w, "player paddle"))->position = {-1.0f, 0.5f, 8.4f};
    w.get<Transform>(named(w, "opponent paddle"))->position = {0.8f, -0.4f, -8.4f};
}

struct SaveSnapshotCheck {
    bool file_written = false;
    pong::Match restored{};
    Vec3 ball_pos{}, player_pos{}, opponent_pos{};
    Vec3 player_pip2{}, player_pip3{}, opp_pip1{}, opp_pip2{};
    int player_children = 0, opp_children = 0;
    std::size_t paddles = 0, balls = 0, pips = 0, courts = 0, sounds = 0;
};

void capture_snapshot(World& w, SaveSnapshotCheck& out) {
    out.restored = *pong::match(w);
    out.ball_pos = w.get<Transform>(named(w, "ball"))->position;
    out.player_pos = w.get<Transform>(named(w, "player paddle"))->position;
    out.opponent_pos = w.get<Transform>(named(w, "opponent paddle"))->position;
    out.player_children = count_children_of(w, named(w, "player paddle"));
    out.opp_children = count_children_of(w, named(w, "opponent paddle"));
    out.paddles = w.count<pong::Paddle>();
    out.balls = w.count<pong::Ball>();
    out.pips = w.count<pong::ScorePip>();
    out.courts = w.count<pong::Court>();
    out.sounds = w.count<pong::Sounds>();
    w.each<const pong::ScorePip, const PrimitiveMesh>([&](const pong::ScorePip& p, const PrimitiveMesh& m) {
        if (p.side == pong::Side::Player && p.index == 2) out.player_pip2 = m.material.emissive;
        if (p.side == pong::Side::Player && p.index == 3) out.player_pip3 = m.material.emissive;
        if (p.side == pong::Side::Opponent && p.index == 1) out.opp_pip1 = m.material.emissive;
        if (p.side == pong::Side::Opponent && p.index == 2) out.opp_pip2 = m.material.emissive;
    });
}

void scramble_match(World& w) {
    pong::Match& m = *w.get<pong::Match>(named(w, "court"));
    m.player = 0;
    m.opponent = 4;
    m.rallies = 99;
    w.get<Transform>(named(w, "ball"))->position = {-2.5f, 1.2f, -4.0f};
    w.get<Transform>(named(w, "player paddle"))->position = {-2.0f, -1.0f, 8.4f};
}

} // namespace

TEST_CASE("save and load match slots restore state, pips, and scene integrity") {
    TempSaveDir saves("astral_pong_save_slot_test");
    const std::string dir = saves.path.string();
    const InputScriptEvent script[] = {
        {.frame = 5, .key = Key::F5, .down = true},
        {.frame = 6, .key = Key::F5, .down = false},
        {.frame = 20, .key = Key::F9, .down = true},
        {.frame = 21, .key = Key::F9, .down = false},
    };
    App app = make_app(dir, script);
    Run run;
    run.player_ai = false;
    SaveSnapshotCheck s;
    run.setup = [&](Context& ctx) { ctx.world.get<pong::Paddle>(named(ctx.world, "opponent paddle"))->ai = false; };
    run.each_frame = [&](Context& ctx) {
        if (ctx.time.frame == 5) seed_custom_match(ctx.world);
        if (ctx.time.frame == 7) s.file_written = std::filesystem::exists(saves.path / "match.json");
        if (ctx.time.frame == 15) scramble_match(ctx.world);
        if (ctx.time.frame == 21) capture_snapshot(ctx.world, s);
    };
    app.run_frames(run, 22);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(s.file_written);
    CHECK(s.restored.player == 3);
    CHECK(s.restored.opponent == 2);
    CHECK(s.restored.rallies == 12);
    CHECK(s.restored.serve_rng == 12345);
    CHECK(s.restored.ai_rng == 67890);
    CHECK(s.ball_pos.x == doctest::Approx(1.1f + 2.0f / 60.0f).epsilon(0.01));
    CHECK(s.ball_pos.y == doctest::Approx(-0.4f - 1.0f / 60.0f).epsilon(0.01));
    CHECK(s.player_pos.x == doctest::Approx(-1.0f));
    CHECK(s.player_pos.y == doctest::Approx(0.5f));
    CHECK(s.opponent_pos.x == doctest::Approx(0.8f));
    CHECK(s.opponent_pos.y == doctest::Approx(-0.4f));
    CHECK(s.paddles == 2);
    CHECK(s.balls == 1);
    CHECK(s.pips == 10);
    CHECK(s.courts == 1);
    CHECK(s.sounds == 1);
    CHECK(s.player_children == 4);
    CHECK(s.opp_children == 5);
    CHECK(s.player_pip2.z > 1.0f);
    CHECK(s.player_pip3.z < 0.2f);
    CHECK(s.opp_pip1.x > 1.0f);
    CHECK(s.opp_pip2.x < 0.2f);
}

TEST_CASE("a saved match reloads into a new App session from the save folder") {
    TempSaveDir saves("astral_pong_cross_session_test");
    const std::string dir = saves.path.string();
    const InputScriptEvent save_script[] = {
        {.frame = 3, .key = Key::F5, .down = true},
        {.frame = 4, .key = Key::F5, .down = false},
    };
    {
        App app = make_app(dir, save_script);
        Run run;
        run.player_ai = false;
        run.setup = [&](Context& ctx) { ctx.world.get<pong::Paddle>(named(ctx.world, "opponent paddle"))->ai = false; };
        run.each_frame = [&](Context& ctx) {
            if (ctx.time.frame == 3) seed_custom_match(ctx.world);
        };
        app.run_frames(run, 6);
        CHECK(app.context().scripts->take_errors().empty());
    }
    const InputScriptEvent load_script[] = {
        {.frame = 2, .key = Key::F9, .down = true},
        {.frame = 3, .key = Key::F9, .down = false},
    };
    App app2 = make_app(dir, load_script);
    Run run2;
    run2.player_ai = false;
    pong::Match reloaded{};
    run2.setup = [&](Context& ctx) { ctx.world.get<pong::Paddle>(named(ctx.world, "opponent paddle"))->ai = false; };
    run2.each_frame = [&](Context& ctx) {
        if (ctx.time.frame == 3) reloaded = *pong::match(ctx.world);
    };
    app2.run_frames(run2, 4);
    CHECK(app2.context().scripts->take_errors().empty());
    CHECK(reloaded.player == 3);
    CHECK(reloaded.opponent == 2);
    CHECK(reloaded.rallies == 12);
    CHECK(reloaded.serve_rng == 12345);
    CHECK(reloaded.ai_rng == 67890);
}

TEST_CASE("loading with no existing save slot leaves the match unchanged and error-free") {
    TempSaveDir saves("astral_pong_empty_slot_test");
    const InputScriptEvent script[] = {
        {.frame = 2, .key = Key::F9, .down = true},
        {.frame = 3, .key = Key::F9, .down = false},
    };
    App app = make_app(saves.path.string(), script);
    Run run;
    run.player_ai = false;
    pong::Match after{};
    run.setup = [&](Context& ctx) { ctx.world.get<pong::Paddle>(named(ctx.world, "opponent paddle"))->ai = false; };
    run.each_frame = [&](Context& ctx) {
        if (ctx.time.frame == 1) ctx.world.get<pong::Match>(named(ctx.world, "court"))->player = 2;
        if (ctx.time.frame == 4) after = *pong::match(ctx.world);
    };
    app.run_frames(run, 5);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(after.player == 2);
}

TEST_CASE("gamepad input actions move the player paddle and trigger save/load") {
    TempSaveDir saves("astral_pong_gamepad_test");
    App app = make_app(saves.path.string());
    Run run;
    run.player_ai = false;
    Input pad;
    Vec3 after_left_stick{}, after_deadzone{}, after_dpad{}, after_right_stick{};
    bool saved_by_pad = false;
    pong::Match after_pad_load{};
    run.setup = [&](Context& ctx) { ctx.world.get<pong::Paddle>(named(ctx.world, "opponent paddle"))->ai = false; };
    run.each_frame = [&](Context& ctx) {
        World& w = ctx.world;
        const std::uint64_t f = ctx.time.frame;
        pad.begin_frame();
        pad.set_gamepad_connected(true);
        pad.set_gamepad_axis(GamepadAxis::LeftX, (f >= 1 && f <= 15) ? 1.0f : (f >= 16 && f <= 30) ? 0.05f : 0.0f);
        pad.set_gamepad_axis(GamepadAxis::LeftY, (f >= 1 && f <= 15) ? 0.5f : (f >= 16 && f <= 30) ? 0.05f : 0.0f);
        pad.set_gamepad_button(GamepadButton::DpadLeft, f >= 31 && f <= 45);
        pad.set_gamepad_button(GamepadButton::DpadDown, f >= 31 && f <= 45);
        pad.set_gamepad_axis(GamepadAxis::RightY, (f >= 46 && f <= 60) ? 1.0f : 0.0f);
        pad.set_gamepad_button(GamepadButton::LeftShoulder, f == 62);
        pad.set_gamepad_button(GamepadButton::RightShoulder, f == 65);
        ctx.actions.update(pad);

        const Vec3 p = w.get<Transform>(named(w, "player paddle"))->position;
        if (f == 16) after_left_stick = p;
        if (f == 31) after_deadzone = p;
        if (f == 46) after_dpad = p;
        if (f == 61) after_right_stick = p;
        if (f == 62) {
            pong::Match& m = *w.get<pong::Match>(named(w, "court"));
            m.player = 2;
            m.opponent = 1;
        }
        if (f == 64) {
            saved_by_pad = std::filesystem::exists(saves.path / "match.json");
            pong::Match& m = *w.get<pong::Match>(named(w, "court"));
            m.player = 0;
            m.opponent = 4;
        }
        if (f == 66) after_pad_load = *pong::match(w);
    };
    app.run_frames(run, 67);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(after_left_stick.x > 1.0f);
    CHECK(after_left_stick.y > 0.5f);
    CHECK(after_deadzone.x == doctest::Approx(after_left_stick.x));
    CHECK(after_deadzone.y == doctest::Approx(after_left_stick.y));
    CHECK(after_dpad.x < after_deadzone.x - 1.0f);
    CHECK(after_dpad.y < after_deadzone.y - 1.0f);
    CHECK(after_right_stick.y > after_dpad.y + 1.0f);
    CHECK(saved_by_pad);
    CHECK(after_pad_load.player == 2);
    CHECK(after_pad_load.opponent == 1);
}
