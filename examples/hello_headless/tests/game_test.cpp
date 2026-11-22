// hello_headless: the counter counts, deterministically.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"
#include "features/tally/tally.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("120 frames at 60 Hz are exactly two seconds") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
    hello_headless::HeadlessGame game;
    app.run_frames(game, 120);
    const tally::Tally* t = app.world().get<const tally::Tally>(find_by_name(app.world(), "counter"));
    REQUIRE(t);
    CHECK(t->frames == 120);
    CHECK(t->seconds == doctest::Approx(2.0f).epsilon(0.001));
    CHECK(app.context().scripts->take_errors().empty());
}
