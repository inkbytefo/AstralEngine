#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "game.hpp"

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("vfx_lab loads courtyard.scene, spawns colliders, and plays VFX or GameplaySmoke") {
    {
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
        vfx_lab::VfxLabGame game({.vfx = true});
        app.run_frames(game, 10);
        CHECK(app.world().count<Camera>() >= 1);
        CHECK(app.context().physics->body_count() > 0);
        CHECK(app.world().count<GameplaySmoke>() >= 1);
    }
    {
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = ASTRAL_GAME_ASSETS, .hot_reload = false});
        vfx_lab::VfxLabGame game({.hour = 18.0f, .fog = 0.02f, .smoke = true});
        app.run_frames(game, 5);
        CHECK(app.world().count<GameplaySmoke>() == 1);
        const Atmosphere* atmo = app.context().singleton<Atmosphere>();
        if (atmo) CHECK(atmo->hour == doctest::Approx(18.0f));
    }
}
