// sdf_scene, headless: the lights orbit and the torus tumbles by the Luau rules.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"
#include "features/orbits/orbits.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("the orbiting things move on circles; the torus turns; no script errors") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
    sdf_scene::SdfGame game;
    app.run_frames(game, 60);
    World& w = app.world();
    CHECK(w.count<orbits::Orbit>() == 6);
    bool lights_on_circle = true;
    w.each<const Transform, const orbits::Orbit, const PointLight>([&](const Transform& t, const orbits::Orbit& o, const PointLight&) {
        lights_on_circle = lights_on_circle && std::abs(std::sqrt(t.position.x * t.position.x + t.position.z * t.position.z) - o.radius) < 1e-3f;
    });
    CHECK(lights_on_circle);
    float turned = 0;
    w.each<const Transform, const orbits::Tumble>([&](const Transform& t, const orbits::Tumble&) { turned = std::abs(t.rotation.w - 1.0f); });
    CHECK(turned > 0.0f);
    CHECK(app.context().scripts->take_errors().empty());
}
