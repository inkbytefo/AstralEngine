// spinning_cubes, headless: the grid exists, the cubes spin (C++) and bob (Luau), the camera flies.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"
#include "features/cubes/cubes.hpp"
#include "features/fly_camera/fly_camera.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("cubes spin and bob; the free camera is there; no script errors") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
    spinning_cubes::CubesGame game;
    app.run_frames(game, 30);
    World& w = app.world();
    CHECK(w.count<cubes::Spin>() == 24 * 24);
    CHECK(w.count<fly::FlyCamera>() == 1);
    float lowest = 1e9f, highest = -1e9f;
    float turned = 0;
    w.each<const Transform, const cubes::Bob>([&](const Transform& t, const cubes::Bob&) {
        lowest = std::min(lowest, t.position.y);
        highest = std::max(highest, t.position.y);
    });
    w.each<const Transform, const cubes::Spin>([&](const Transform& t, const cubes::Spin&) { turned = std::max(turned, std::abs(t.rotation.w - 1.0f)); });
    CHECK(highest - lowest > 0.2f); // the phases differ: some are up, some are down
    CHECK(turned > 0.0f);
    CHECK(app.context().scripts->take_errors().empty());
}
