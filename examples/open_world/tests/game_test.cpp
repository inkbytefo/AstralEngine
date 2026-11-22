// open_world, headless: the map opens and streams, the camera is a streaming source, N flips night.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"
#include "features/day_night/day_night.hpp"
#include "features/fly_camera/fly_camera.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("the world opens around the camera; the day/night state drives the map's night layer") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_OPEN_WORLD_ASSETS, .hot_reload = false});
    open_world::OpenWorldGame game(true, true); // capturing mode: autopilot and loading every frame; start at night
    app.run_frames(game, 10);
    Context& ctx = app.context();
    REQUIRE(ctx.world_streaming());
    CHECK(ctx.world_streaming()->attached > 0);
    CHECK(app.world().count<fly::FlyCamera>() == 1);
    CHECK(app.world().count<StreamingSource>() == 1);
    CHECK(ctx.sun.intensity == doctest::Approx(0.08f)); // night
    CHECK(ctx.world_layers()->active("night"));
    CHECK(ctx.scripts->take_errors().empty());
}
