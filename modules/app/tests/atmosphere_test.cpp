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

TEST_CASE("G1.1 default artistic controls keep the physical day cycle unchanged at 0/6/9/12/18 h") {
    struct Expected {
        float hour;
        Vec3 toward, transmission, sky;
        float intensity;
    };
    // Reference values of the pre-G1.1 model (orbit tilt 25 deg, heading 0, intensity 3).
    const Expected cases[] = {
        {0, {0, -0.906308f, -0.422618f}, {0.0742736f, 0.0100518f, 0.000101039f}, {0.008f, 0.014f, 0.032f}, 0},
        {6, {-1, 0, 0}, {0.0742736f, 0.0100518f, 0.000101039f}, {0.13625f, 0.20375f, 0.3425f}, 0},
        {9, {-0.707107f, 0.640856f, 0.298836f}, {0.90355f, 0.835737f, 0.698456f}, {0.35f, 0.52f, 0.86f}, 3},
        {12, {0, 0.906308f, 0.422618f}, {0.930792f, 0.880832f, 0.775865f}, {0.35f, 0.52f, 0.86f}, 3},
        {18, {1, 0, 0}, {0.0742736f, 0.0100518f, 0.000101039f}, {0.13625f, 0.20375f, 0.3425f}, 0},
    };
    auto near = [](Vec3 a, Vec3 b) {
        CHECK(a.x == doctest::Approx(b.x).epsilon(1e-4));
        CHECK(a.y == doctest::Approx(b.y).epsilon(1e-4));
        CHECK(a.z == doctest::Approx(b.z).epsilon(1e-4));
    };
    for (const Expected& e : cases) {
        CAPTURE(e.hour);
        const auto lit = evaluate_atmosphere(Atmosphere{.hour = e.hour, .sky_material = {}});
        near(lit.toward_sun, e.toward);
        near(lit.transmission, e.transmission);
        near(lit.sky, e.sky);
        CHECK(lit.sun_intensity == doctest::Approx(e.intensity).epsilon(1e-4));
    }
}

TEST_CASE("G1.1 twilight colour, horizon tint and sky tint shape the low-sun look only") {
    const Atmosphere plain{.hour = 6.25f, .sky_material = {}};
    Atmosphere styled = plain;
    styled.twilight_color = {0.9f, 0.4f, 0.2f};
    styled.twilight_strength = 1.0f;
    styled.horizon_tint = {1.0f, 0.5f, 0.25f};
    const auto base = evaluate_atmosphere(plain);
    const auto dawn = evaluate_atmosphere(styled);
    CHECK(dawn.sky.x > base.sky.x);           // pulled toward the warm twilight colour
    CHECK(dawn.sky.z < base.sky.z);
    CHECK(dawn.transmission.y < base.transmission.y); // low sun takes the horizon tint
    CHECK(dawn.transmission.x == doctest::Approx(base.transmission.x));
    CHECK(dawn.toward_sun == base.toward_sun);

    styled.hour = 12.0f; // high sun: twilight and horizon tint fade out
    const auto noon_plain = evaluate_atmosphere(Atmosphere{.hour = 12.0f, .sky_material = {}});
    const auto noon = evaluate_atmosphere(styled);
    CHECK(noon.sky == noon_plain.sky);
    CHECK(noon.transmission == noon_plain.transmission);

    styled.sky_tint = {0.5f, 1.0f, 2.0f}; // sky tint always multiplies the ambient sky
    const auto tinted = evaluate_atmosphere(styled);
    CHECK(tinted.sky.x == doctest::Approx(noon_plain.sky.x * 0.5f));
    CHECK(tinted.sky.z == doctest::Approx(noon_plain.sky.z * 2.0f));
}

TEST_CASE("G1.1 drive_sun / drive_sky off leave the game's own sun and sky untouched") {
    struct G final : Game {
        bool checked = false;
        void on_start(Context& ctx) override {
            ctx.sun.direction = {0, -1, 0};
            ctx.sun.color = {0.1f, 0.2f, 0.3f};
            ctx.sun.intensity = 7.0f;
            ctx.environment.sky = {0.4f, 0.5f, 0.6f};
            ctx.world.create(Atmosphere{.hour = 9, .sky_material = {}, .drive_sun = false});
        }
        void on_update(Context& ctx) override {
            if (ctx.time.frame != 1) return;
            checked = true;
            CHECK(ctx.sun.intensity == doctest::Approx(7.0f));
            CHECK(ctx.sun.color == Vec3{0.1f, 0.2f, 0.3f});
            CHECK(ctx.environment.sky == Vec3{0.35f, 0.52f, 0.86f}); // sky still driven
            ctx.world.each<Atmosphere>([](Atmosphere& a) {
                a.drive_sun = true;
                a.drive_sky = false;
            });
            ctx.environment.sky = {0.4f, 0.5f, 0.6f};
        }
    } game;
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f});
    app.run_frames(game, 3);
    REQUIRE(game.checked);
    CHECK(app.context().environment.sky == Vec3{0.4f, 0.5f, 0.6f});
    CHECK(app.context().sun.intensity == doctest::Approx(3.0f));
}
