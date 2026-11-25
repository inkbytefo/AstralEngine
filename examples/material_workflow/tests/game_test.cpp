#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("material_workflow loads materials.scene and resolves referenced models headlessly") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
    material_workflow::WorkflowGame game;
    app.run_frames(game, 2);
    CHECK(app.world().count<ModelRef>() > 0);
    CHECK(app.world().count<Camera>() >= 1);
    CHECK(app.world().count<MeshRenderer>() > 0);
}
