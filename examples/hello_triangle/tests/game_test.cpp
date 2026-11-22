// hello_triangle, headless: the mesh is a valid triangle and the scene has one drawable and a camera.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("one triangle, one drawable, one camera") {
    const MeshData mesh = hello_triangle::triangle_mesh();
    CHECK(mesh.vertices.size() == 3);
    CHECK(mesh.indices.size() == 3);
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
    hello_triangle::TriangleGame game;
    app.run_frames(game, 2);
    CHECK(app.world().count<MeshRenderer>() == 1);
    CHECK(app.world().count<Camera>() == 1);
}
