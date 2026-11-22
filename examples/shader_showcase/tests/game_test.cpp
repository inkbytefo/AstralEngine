// shader_showcase, headless: the scene is built and the free camera is installed.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"
#include "features/fly_camera/fly_camera.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("the showcase scene has its material-file surfaces, a light and a flyable camera") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
    shader_showcase::ShowcaseGame game;
    app.run_frames(game, 3);
    World& w = app.world();
    std::size_t from_files = 0;
    w.each<const PrimitiveMesh>([&](const PrimitiveMesh& m) { from_files += m.material_file.empty() ? 0 : 1; });
    CHECK(from_files == 9); // 5 leaves, 2 glass spheres, lava, hologram
    CHECK(w.count<PointLight>() == 1);
    CHECK(w.count<fly::FlyCamera>() == 1);
    CHECK(app.context().scripts->take_errors().empty());
}
