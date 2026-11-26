// world_showcase, headless. Covers open_world (4x4 km cell streaming, floating origin, night layer, --nudge),
// terrain_lab (2x2 hill near and --far views, collision), streaming 6x6 range terrain, station switching without
// leftovers, HUD/debug toggles, and loose-vs-package parity.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "features/day_night/day_night.hpp"
#include "features/fly_camera/fly_camera.hpp"
#include "game.hpp"
#include "stations/stations.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <vector>

using namespace astral;
using world_showcase::Options;
using world_showcase::ShowcaseGame;
using world_showcase::StationId;

namespace {

AppDesc showcase_desc() {
    AppDesc d{};
    d.headless = true;
    d.asset_root = ASTRAL_GAME_ASSETS;
    d.hot_reload = false;
    d.fixed_timestep = 1.0f / 60.0f;
    return d;
}

void tap(std::vector<InputScriptEvent>& script, std::uint64_t frame, Key key) {
    script.push_back({.frame = frame, .key = key, .down = true});
    script.push_back({.frame = frame + 2, .key = key, .down = false});
}

template <class Probe> struct Probed final : Game {
    ShowcaseGame inner;
    Probe probe;
    Probed(Options options, Probe p) : inner(options), probe(p) {}
    void on_start(Context& ctx) override { inner.on_start(ctx); }
    void on_update(Context& ctx) override {
        inner.on_update(ctx);
        probe(ctx.time.frame, ctx, inner);
    }
};

Vec3 camera_world_pos(Context& ctx) {
    Vec3 local{};
    ctx.world.each<const Transform, const Camera>([&](const Transform& t, const Camera&) { local = t.position; });
    const WorldPosition o = ctx.maps().world_origin();
    return {static_cast<float>(o.x + local.x), static_cast<float>(o.y + local.y), static_cast<float>(o.z + local.z)};
}

} // namespace

TEST_CASE("world station: streams cells around camera and shifts the floating origin past 2 km") {
    App app(showcase_desc());
    ShowcaseGame game({.station = StationId::World, .capturing = true});
    app.run_frames(game, 10);
    Context& ctx = app.context();
    CHECK_FALSE(ctx.quit_requested());
    CHECK(ctx.scripts->take_errors().empty());
    REQUIRE(ctx.maps().world_streaming() != nullptr);
    CHECK(ctx.maps().world_streaming()->attached > 0);
    CHECK(app.world().count<fly::FlyCamera>() == 1);
    CHECK(app.world().count<StreamingSource>() == 1);
    CHECK(app.world().count<Camera>() == 1);
    CHECK(ctx.maps().world_origin().x == doctest::Approx(1700.0));
    CHECK(ctx.maps().world_origin().z == doctest::Approx(1700.0));
    CHECK(game.origin_shifts() >= 1u);
}

TEST_CASE("world station --night and N key toggle the night data layer, lamps and lighting") {
    std::vector<InputScriptEvent> script;
    tap(script, 6, Key::N);
    AppDesc desc = showcase_desc();
    desc.input_script = script;
    App app(desc);

    bool night_layer_before = false;
    float sun_before = 0.0f;
    std::size_t lights_before = 0;
    bool night_layer_after = true;
    float sun_after = 0.0f;
    std::size_t lights_after = 999;

    Probed game({.station = StationId::World, .capturing = true, .night = true},
                [&](std::uint64_t frame, Context& ctx, ShowcaseGame&) {
                    if (frame == 4) {
                        REQUIRE(ctx.maps().world_layers() != nullptr);
                        night_layer_before = ctx.maps().world_layers()->active("night");
                        sun_before = ctx.sun.intensity;
                        lights_before = ctx.world.count<PointLight>();
                    }
                    if (frame == 12) {
                        REQUIRE(ctx.maps().world_layers() != nullptr);
                        night_layer_after = ctx.maps().world_layers()->active("night");
                        sun_after = ctx.sun.intensity;
                        lights_after = ctx.world.count<PointLight>();
                    }
                });
    app.run_frames(game, 14);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(night_layer_before);
    CHECK(sun_before == doctest::Approx(0.08f));
    CHECK(lights_before > 0u);
    CHECK_FALSE(night_layer_after);
    CHECK(sun_after == doctest::Approx(3.0f));
    CHECK(lights_after == 0u);
}

TEST_CASE("world station --nudge offsets the start Z without disturbing cell streaming") {
    const auto run = [](float nudge) {
        App app(showcase_desc());
        ShowcaseGame game({.station = StationId::World, .capturing = false, .nudge = nudge});
        app.run_frames(game, 4);
        REQUIRE(app.context().maps().world_streaming() != nullptr);
        return std::pair{camera_world_pos(app.context()), app.context().maps().world_streaming()->attached};
    };
    const auto [p0, cells0] = run(0.0f);
    const auto [p1, cells1] = run(0.0003f);
    CHECK(cells0 > 0u);
    CHECK(cells1 == cells0);
    CHECK(std::abs((p1.z - p0.z) - 0.0003f) < 1e-4f);
}

TEST_CASE("range station: streams 6x6 terrain tiles within radius and swaps colliders as camera moves") {
    App app(showcase_desc());
    std::size_t bodies_start = 0;
    bool hit_start = false;
    bool far_unloaded_at_start = false;
    bool far_loaded_after_move = false;
    bool start_unloaded_after_move = false;

    Probed game({.station = StationId::Range, .capturing = false}, [&](std::uint64_t frame, Context& ctx, ShowcaseGame&) {
        if (frame == 2) {
            bodies_start = ctx.physics->body_count();
            hit_start = ctx.physics->raycast({300.0f, 150.0f, 540.0f}, {0, -1, 0}, 200.0f).has_value();
            far_unloaded_at_start = !ctx.physics->raycast({64.0f, 150.0f, 64.0f}, {0, -1, 0}, 200.0f).has_value();
            ctx.world.each<Transform, Camera>([](Transform& t, Camera&) { t.position = {64.0f, 90.0f, 64.0f}; });
            ctx.maps().finish_world_loading();
        }
        if (frame == 4) {
            far_loaded_after_move = ctx.physics->raycast({64.0f, 150.0f, 64.0f}, {0, -1, 0}, 200.0f).has_value();
            start_unloaded_after_move = !ctx.physics->raycast({700.0f, 150.0f, 700.0f}, {0, -1, 0}, 200.0f).has_value();
        }
    });
    app.run_frames(game, 6);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(app.world().count<TerrainRef>() == 1u);
    CHECK(bodies_start > 0u);
    CHECK(bodies_start < 36u);
    CHECK(hit_start);
    CHECK(far_unloaded_at_start);
    CHECK(far_loaded_after_move);
    CHECK(start_unloaded_after_move);
}

TEST_CASE("hill station: loads terrain.scene and streams collision headlessly for near and --far views") {
    for (const bool far_view : {false, true}) {
        App app(showcase_desc());
        ShowcaseGame game({.station = StationId::Hill, .far_view = far_view});
        app.run_frames(game, 5);
        CHECK_FALSE(app.context().quit_requested());
        CHECK(app.context().scripts->take_errors().empty());
        CHECK(app.world().count<TerrainRef>() >= 1u);
        CHECK(app.world().count<Camera>() >= 1u);
        CHECK(app.context().physics->body_count() == 4u);
        Vec3 eye{};
        app.world().each<const Transform, const Camera>([&](const Transform& t, const Camera&) { eye = t.position; });
        const Vec3 expected = world_showcase::hill_camera_eye(far_view);
        CHECK(length(eye - expected) < 0.01f);
        CHECK(app.context().physics->raycast({64.0f, 50.0f, 64.0f}, {0, -1, 0}, 80.0f).has_value());
    }
}

TEST_CASE("number keys switch stations cleanly and leave no map, terrain or collider leftovers") {
    std::vector<InputScriptEvent> script;
    tap(script, 6, Key::Num2);
    tap(script, 14, Key::Num3);
    tap(script, 22, Key::Num1);
    AppDesc desc = showcase_desc();
    desc.input_script = script;
    App app(desc);

    std::vector<StationId> seen;
    bool range_has_no_map = false;
    bool hill_has_no_map = false;
    std::size_t hill_bodies = 0;
    std::size_t world_terrains_after = 99;
    std::size_t world_bodies_after = 99;
    std::uint32_t world_cells_after = 0;
    Vec3 world_pos_after{};

    Probed game({.station = StationId::World, .capturing = false},
                [&](std::uint64_t frame, Context& ctx, ShowcaseGame& g) {
                    if (frame == 10) {
                        seen.push_back(g.station());
                        range_has_no_map = (ctx.maps().world_streaming() == nullptr);
                    } else if (frame == 18) {
                        seen.push_back(g.station());
                        hill_has_no_map = (ctx.maps().world_streaming() == nullptr);
                        hill_bodies = ctx.physics->body_count();
                    } else if (frame == 26) {
                        seen.push_back(g.station());
                        world_terrains_after = ctx.world.count<TerrainRef>();
                        world_bodies_after = ctx.physics->body_count();
                        world_cells_after = ctx.maps().world_streaming() ? ctx.maps().world_streaming()->attached : 0u;
                        world_pos_after = camera_world_pos(ctx);
                    }
                });
    app.run_frames(game, 28);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(seen == std::vector<StationId>{StationId::Range, StationId::Hill, StationId::World});
    CHECK(range_has_no_map);
    CHECK(hill_has_no_map);
    CHECK(hill_bodies == 4u);
    CHECK(world_terrains_after == 0u);
    CHECK(world_bodies_after == 0u);
    CHECK(world_cells_after > 0u);
    CHECK(world_pos_after.x == doctest::Approx(1700.0f).epsilon(1.0f));
    CHECK(world_pos_after.z == doctest::Approx(1700.0f).epsilon(1.0f));
}

TEST_CASE("C toggles world cell debug overlay and F toggles hill far camera") {
    std::vector<InputScriptEvent> script;
    tap(script, 4, Key::C);
    tap(script, 10, Key::Num3);
    tap(script, 16, Key::F);
    AppDesc desc = showcase_desc();
    desc.input_script = script;
    App app(desc);

    bool cells_after_c = false;
    bool far_after_f = false;
    Vec3 hill_eye_after_f{};

    Probed game({.station = StationId::World}, [&](std::uint64_t frame, Context& ctx, ShowcaseGame& g) {
        if (frame == 7) cells_after_c = ctx.debug_show.world_cells;
        if (frame == 20) {
            far_after_f = g.far_view();
            ctx.world.each<const Transform, const Camera>([&](const Transform& t, const Camera&) { hill_eye_after_f = t.position; });
        }
    });
    app.run_frames(game, 22);
    CHECK(cells_after_c);
    CHECK(far_after_f);
    CHECK(length(hill_eye_after_f - world_showcase::hill_camera_eye(true)) < 0.01f);
}

TEST_CASE("station names from command line; unknown defaults to world and bare --far opens hill") {
    CHECK(world_showcase::station_from_name("world") == StationId::World);
    CHECK(world_showcase::station_from_name("range") == StationId::Range);
    CHECK(world_showcase::station_from_name("hill") == StationId::Hill);
    CHECK(world_showcase::station_from_name("unknown") == StationId::World);
    CHECK(world_showcase::resolve_initial_station("", false) == StationId::World);
    CHECK(world_showcase::resolve_initial_station("", true) == StationId::Hill);
    CHECK(world_showcase::resolve_initial_station("range", true) == StationId::Range);
}

TEST_CASE("loose assets and the cooked package behave the same for streamed world and terrain") {
    struct Snapshot {
        std::uint32_t world_cells = 0;
        double origin_x = 0.0;
        double origin_z = 0.0;
        std::size_t range_bodies = 0;
        float range_height = -1.0f;
    };
    const auto run = [](bool packed) {
        Snapshot snap;
        {
            AppDesc desc = showcase_desc();
            if (packed) {
                desc.pack = ASTRAL_SHOWCASE_TEST_PACK;
                REQUIRE(std::filesystem::is_regular_file(desc.pack));
                desc.asset_root = "no-loose-assets-here";
            }
            App app(desc);
            CHECK(app.context().assets->from_pack() == packed);
            ShowcaseGame game({.station = StationId::World, .capturing = true, .night = true});
            app.run_frames(game, 10);
            CHECK(app.context().scripts->take_errors().empty());
            REQUIRE(app.context().maps().world_streaming() != nullptr);
            snap.world_cells = app.context().maps().world_streaming()->attached;
            snap.origin_x = app.context().maps().world_origin().x;
            snap.origin_z = app.context().maps().world_origin().z;
        }
        {
            AppDesc desc = showcase_desc();
            if (packed) {
                desc.pack = ASTRAL_SHOWCASE_TEST_PACK;
                desc.asset_root = "no-loose-assets-here";
            }
            App app(desc);
            CHECK(app.context().assets->from_pack() == packed);
            ShowcaseGame game({.station = StationId::Range, .capturing = false});
            app.run_frames(game, 5);
            CHECK(app.context().scripts->take_errors().empty());
            snap.range_bodies = app.context().physics->body_count();
            const auto hit = app.context().physics->raycast({300.0f, 150.0f, 540.0f}, {0, -1, 0}, 200.0f);
            REQUIRE(hit);
            snap.range_height = hit->position.y;
        }
        return snap;
    };
    const Snapshot loose = run(false);
    const Snapshot packed = run(true);
    CHECK(loose.world_cells > 0u);
    CHECK(packed.world_cells == loose.world_cells);
    CHECK(packed.origin_x == doctest::Approx(loose.origin_x));
    CHECK(packed.origin_z == doctest::Approx(loose.origin_z));
    CHECK(loose.range_bodies > 0u);
    CHECK(packed.range_bodies == loose.range_bodies);
    CHECK(packed.range_height == doctest::Approx(loose.range_height).epsilon(1e-3f));
}
