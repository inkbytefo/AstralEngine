// The game, headless and deterministic: install the features, load the scene, run frames, look at the world.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"

#include <doctest/doctest.h>

using namespace astral;

namespace {

App make_app() {
    return App({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
}

} // namespace

TEST_CASE("the cube spins: the Luau rule turns every Spin") {
    App app = make_app();
    blank::BlankGame game;
    app.run_frames(game, 60); // one second
    const Entity cube = find_by_name(app.world(), "cube");
    REQUIRE(cube.valid());
    const Transform* t = app.world().get<Transform>(cube);
    CHECK(t->rotation.w < 0.9f); // no longer the identity: it turned about 1.2 rad
    CHECK(length(Vec3{t->rotation.x, t->rotation.y, t->rotation.z}) > 0.3f);
}

TEST_CASE("every feature installs; the scripts have no errors") {
    App app = make_app();
    struct Check final : Game {
        bool installed = false;
        void on_start(Context& ctx) override { installed = blank::install_features(ctx); }
    } check;
    app.run_frames(check, 1);
    CHECK(check.installed);
    CHECK(app.context().scripts->take_errors().empty());
}
