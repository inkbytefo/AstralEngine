#include "../src/services/atmosphere.hpp"

#include "../../assets/tests/gltf_writer.hpp"

#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <fstream>

using namespace astral;

TEST_CASE("solar lighting follows the clock and reddens toward the horizon") {
    const auto morning = evaluate_atmosphere(Atmosphere{.hour = 7, .sky_material = {}});
    const auto noon = evaluate_atmosphere(Atmosphere{.hour = 12, .sky_material = {}});
    const auto evening = evaluate_atmosphere(Atmosphere{.hour = 17, .sky_material = {}});
    const auto night = evaluate_atmosphere(Atmosphere{.hour = -1, .sky_material = {}});
    CHECK(length(noon.toward_sun) == doctest::Approx(1));
    CHECK(morning.toward_sun.x < 0);
    CHECK(evening.toward_sun.x > 0);
    CHECK(noon.toward_sun.y > evening.toward_sun.y);
    CHECK(noon.transmission.z > evening.transmission.z);
    CHECK(evening.transmission.x > evening.transmission.z);
    CHECK(night.sun_intensity == 0);
    CHECK(night.sky.z < noon.sky.z);
    CHECK(night.toward_sun == evaluate_atmosphere(Atmosphere{.hour = 23, .sky_material = {}}).toward_sun);
}

TEST_CASE("scene atmosphere advances its saved clock and restores authored light when disabled") {
    const auto root = test::temp_dir("app_atmosphere");
    std::ofstream(root / "clock.scene") << R"({
        "format":"astral.scene","version":1,"sun":{"intensity":1.25},"environment":{"sky":[0.2,0.3,0.4],"exposure":1.7},
        "entities":[{"id":1,"components":{"astral::Atmosphere":{"hour":16,"hours_per_second":60}}}]
    })";

    struct ClockGame final : Game {
        Entity clock{};
        unsigned frame = 0;
        void on_start(Context& ctx) override {
            const auto loaded = ctx.scenes().load_scene("clock.scene");
            REQUIRE(loaded.errors.empty());
            REQUIRE(loaded.entities.size() == 1);
            clock = loaded.entities[0];
        }
        void on_update(Context& ctx) override {
            if (++frame == 2) {
                CHECK(ctx.world.get<const Atmosphere>(clock)->hour == doctest::Approx(17));
                CHECK(ctx.sun.direction.x < 0);
                CHECK(ctx.sun.color.x > ctx.sun.color.z);
                CHECK(ctx.environment.exposure == doctest::Approx(1.7));
                REQUIRE(ctx.scenes().save_scene("saved.scene"));
                ctx.world.get<Atmosphere>(clock)->enabled = false;
            } else if (frame == 3) {
                CHECK(ctx.sun.intensity == doctest::Approx(1.25));
                CHECK(ctx.environment.sky == Vec3{0.2f, 0.3f, 0.4f});
                const auto saved = ctx.scenes().load_scene("saved.scene");
                REQUIRE(saved.errors.empty());
                CHECK(ctx.world.get<const Atmosphere>(saved.entities.front())->hour == doctest::Approx(17));
                CHECK(ctx.sun.intensity == doctest::Approx(1.25));
                CHECK(ctx.environment.sky == Vec3{0.2f, 0.3f, 0.4f});
            }
        }
    } game;
    const std::string asset_root = root.string();
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = asset_root, .hot_reload = false});
    app.run_frames(game, 3);
}
