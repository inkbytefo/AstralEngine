// hello_headless: the counter counts, deterministically, with and without AppDesc::input_script.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "features/tally/tally.hpp"
#include "game.hpp"

#include <doctest/doctest.h>

#include <span>

using namespace astral;

namespace {

tally::Tally run_tally(std::uint32_t frames, std::span<const InputScriptEvent> script = {}) {
    App app({.headless = true,
             .fixed_timestep = 1.0f / 60.0f,
             .asset_root = ASTRAL_GAME_ASSETS,
             .hot_reload = false,
             .input_script = script});
    hello_headless::HeadlessGame game;
    app.run_frames(game, frames);
    CHECK(app.context().scripts->take_errors().empty());
    const tally::Tally* t = app.world().get<const tally::Tally>(find_by_name(app.world(), "counter"));
    REQUIRE(t);
    return *t;
}

} // namespace

TEST_CASE("120 frames at 60 Hz are exactly two seconds") {
    const tally::Tally t = run_tally(120);
    CHECK(t.frames == 120);
    CHECK(t.seconds == doctest::Approx(2.0f).epsilon(0.001));
    CHECK_FALSE(t.paused);
    CHECK(t.resets == 0);
    CHECK(t.boosted_frames == 0);
}

TEST_CASE("scripted input drives boost, pause, resume, and reset through tally.actions") {
    // 0..9: normal (+10 -> 10)
    // 10..19: Space held -> boosted (+20 -> 30, boosted_frames = 10)
    // 20..29: P toggles pause on frame 20 (+0 -> 30)
    // 30..39: P toggles resume on frame 30 (+10 -> 40)
    // 40..49: R resets on frame 40, then counts frames 40..49 (+10 -> 10, resets = 1)
    const InputScriptEvent script[] = {
        {.frame = 10, .key = Key::Space, .down = true},
        {.frame = 20, .key = Key::Space, .down = false},
        {.frame = 20, .key = Key::P, .down = true},
        {.frame = 21, .key = Key::P, .down = false},
        {.frame = 30, .key = Key::P, .down = true},
        {.frame = 31, .key = Key::P, .down = false},
        {.frame = 40, .key = Key::R, .down = true},
        {.frame = 41, .key = Key::R, .down = false},
    };
    const tally::Tally t = run_tally(50, script);
    CHECK(t.frames == 10);
    CHECK(t.seconds == doctest::Approx(10.0f / 60.0f).epsilon(0.001));
    CHECK(t.boosted_frames == 10);
    CHECK(t.resets == 1);
    CHECK_FALSE(t.paused);
}

TEST_CASE("demo_input_script exercises boost, pause, and reset and repeats identically") {
    const std::vector<InputScriptEvent> script = hello_headless::demo_input_script();
    REQUIRE_FALSE(script.empty());
    const tally::Tally first = run_tally(120, script);
    const tally::Tally second = run_tally(120, script);
    CHECK(first.frames == 20);
    CHECK(first.seconds == doctest::Approx(20.0f / 60.0f).epsilon(0.001));
    CHECK(first.boosted_frames == 15);
    CHECK(first.resets == 1);
    CHECK_FALSE(first.paused);
    CHECK(second.frames == first.frames);
    CHECK(second.seconds == doctest::Approx(first.seconds));
    CHECK(second.boosted_frames == first.boosted_frames);
    CHECK(second.resets == first.resets);
    CHECK(second.paused == first.paused);
}
