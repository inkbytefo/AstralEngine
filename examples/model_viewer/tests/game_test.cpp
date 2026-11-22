// model_viewer, headless: the studio (floor, key light, camera) around a model reference.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("the studio is built around the model") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .hot_reload = false});
    model_viewer::ViewerGame game("crate.glb");
    app.run_frames(game, 2);
    World& w = app.world();
    CHECK(w.count<ModelRef>() == 1);
    CHECK(w.count<RectLight>() == 1);
    CHECK(w.count<Camera>() == 1);
    CHECK(app.context().sun.intensity == doctest::Approx(1.2f));
}
