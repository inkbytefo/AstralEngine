// scene_viewer, headless: it loads the RGB room scene and its prefabs.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("the RGB room loads with its prefab instances") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
    scene_viewer::ViewerGame game("showcase.scene", false, false);
    app.run_frames(game, 3);
    World& w = app.world();
    CHECK(w.count<Camera>() >= 1);
    CHECK(w.count<PrimitiveMesh>() >= 5);
    CHECK(w.count<RectLight>() + w.count<PointLight>() >= 1);
}
