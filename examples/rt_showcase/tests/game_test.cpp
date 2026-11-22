// rt_showcase, headless: the room is built with its light, SDF shapes and camera.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"
#include "room.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("the room has its meshes, SDF shapes, one panel light and a camera for every view") {
    for (int view = 0; view < 3; ++view) {
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .hot_reload = false});
        rt_showcase::ShowcaseGame game(view);
        app.run_frames(game, 2);
        World& w = app.world();
        CHECK(w.count<WorldTransform, MeshRenderer>() == 10);
        CHECK(w.count<Sdf>() == 3);
        CHECK(w.count<RectLight>() == 1);
        CHECK(w.count<Camera>() == 1);
    }
    CHECK(rt_showcase::room_object_count() == 10);
}
