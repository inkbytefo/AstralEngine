// material_showcase, headless: verifies the 7 M13 PBR spheres, studio geometry, decals, lights, and camera.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "gallery.hpp"
#include "game.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("material showcase spawns all M13 spheres, decals, lights, and camera") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .hot_reload = false});
    material_showcase::ShowcaseGame game;
    app.run_frames(game, 2);
    World& w = app.world();
    CHECK(material_showcase::gallery_sphere_count() == 7u);
    CHECK(w.count<WorldTransform, MeshRenderer>() == 16u); // floor + wall + 7 plinths + 7 spheres
    CHECK(w.count<Decal>() == 2u);
    CHECK(w.count<RectLight>() == 1u);
    CHECK(w.count<PointLight>() == 1u);
    CHECK(w.count<Camera>() == 1u);
}
