// physics_demo, headless: the pyramid stands, balls rain and expire, the player walks, shots fly.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"
#include "features/lifetime/lifetime.hpp"
#include "features/player/player.hpp"
#include "features/rain/rain.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("balls rain, fall and expire by their Lifetime; the player stands on the ground") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
    physics_demo::PhysicsGame game;
    app.run_frames(game, 180); // three seconds
    World& w = app.world();
    CHECK(w.count<lifetime::Lifetime>() >= 4); // about one ball per half second
    float player_y = 1e9f;
    w.each<const CharacterController, const Transform>([&](const CharacterController&, const Transform& t) { player_y = t.position.y; });
    CHECK(player_y > -0.5f);
    CHECK(player_y < 1.0f);
    CHECK(w.count<player::PlayerCamera>() == 1);
    CHECK(w.count<rain::RainSpawner>() == 1);
    CHECK(app.context().scripts->take_errors().empty());
}
