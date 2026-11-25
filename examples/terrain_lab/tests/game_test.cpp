#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("terrain_lab loads terrain.scene and streams collision headlessly") {
    for (const bool far_view : {false, true}) {
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
        terrain_lab::TerrainLabGame game(far_view);
        app.run_frames(game, 5);
        CHECK(app.world().count<TerrainRef>() >= 1);
        CHECK(app.world().count<Camera>() >= 1);
        CHECK(app.context().physics->body_count() > 0);
    }
}
