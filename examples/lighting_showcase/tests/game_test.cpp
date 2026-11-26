// lighting_showcase, headless. The checks of the two examples it replaces (rt_showcase, sdf_scene), the many-lights
// station, switching between stations without leftovers, and the keys.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "features/fly_camera/fly_camera.hpp"
#include "features/orbits/orbits.hpp"
#include "game.hpp"
#include "stations/stations.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <vector>

using namespace astral;
using lighting_showcase::Options;
using lighting_showcase::ShowcaseGame;
using lighting_showcase::StationId;

namespace {

AppDesc showcase_desc() {
    AppDesc d{};
    d.headless = true;
    d.asset_root = ASTRAL_GAME_ASSETS;
    d.hot_reload = false;
    d.fixed_timestep = 1.0f / 60.0f;
    return d;
}

/// What a station leaves in the world and in the Context.
struct Census {
    std::size_t meshes, sdf, rect_lights, point_lights, cameras, fly_cameras, orbits, tumbles;
    float sun, ambient, exposure;
    bool operator==(const Census&) const = default;
};

Census census(App& app) {
    World& w = app.world();
    const Context& ctx = app.context();
    return {w.count<MeshRenderer>(), w.count<Sdf>(),          w.count<RectLight>(),     w.count<PointLight>(),
            w.count<Camera>(),       w.count<fly::FlyCamera>(), w.count<orbits::Orbit>(), w.count<orbits::Tumble>(),
            ctx.sun.intensity,       ctx.environment.ambient,   ctx.environment.exposure};
}

std::vector<Vec3> orbiter_positions(World& w) {
    std::vector<Vec3> out;
    w.each<const Transform, const orbits::Orbit>([&](const Transform& t, const orbits::Orbit&) { out.push_back(t.position); });
    return out;
}

float moved(const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
    float sum = 0.0f;
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) sum += length(a[i] - b[i]);
    return sum;
}

/// Scripted key tap: down on `frame`, up two frames later.
void tap(std::vector<InputScriptEvent>& script, std::uint64_t frame, Key key) {
    script.push_back({.frame = frame, .key = key, .down = true});
    script.push_back({.frame = frame + 2, .key = key, .down = false});
}

/// Runs the game and calls `probe(frame, ctx, game)` after the game's own update.
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

} // namespace

TEST_CASE("room: the RGB room has its meshes, SDF shapes, one panel light, one camera and no sun") {
    App app(showcase_desc());
    ShowcaseGame game({.station = StationId::Room});
    app.run_frames(game, 3);
    CHECK_FALSE(app.context().quit_requested());
    CHECK(app.context().scripts->take_errors().empty());
    const Census c = census(app);
    CHECK(c.meshes == 10);
    CHECK(c.sdf == 3);
    CHECK(c.rect_lights == 1);
    CHECK(c.point_lights == 0);
    CHECK(c.cameras == 1);
    CHECK(c.fly_cameras == 1);
    CHECK(c.sun == 0.0f);
    const GiSettings gi = game.settings().gi;
    CHECK(gi.volume_min.x == doctest::Approx(-3.0f));
    CHECK(gi.volume_max.z == doctest::Approx(5.5f));
}

TEST_CASE("room on the raster path gets a stand-in point light, because rect lights are ray traced only") {
    App app(showcase_desc());
    ShowcaseGame game({.station = StationId::Room, .raster = true});
    app.run_frames(game, 2);
    CHECK(census(app).point_lights == 1);
    CHECK_FALSE(game.settings().ray_tracing);
}

TEST_CASE("yard: SDF shapes under the sun; coloured lights orbit and the torus turns by the Luau rules") {
    App app(showcase_desc());
    ShowcaseGame game({.station = StationId::Yard});
    app.run_frames(game, 60);
    CHECK(app.context().scripts->take_errors().empty());
    const Census c = census(app);
    CHECK(c.meshes == 6); // ground + five gold spheres
    CHECK(c.sdf == 6);    // three blobs, a box, the sphere that carves it, a torus
    CHECK(c.point_lights == 3);
    CHECK(c.rect_lights == 0);
    CHECK(c.orbits == 6);
    CHECK(c.tumbles == 1);
    CHECK(c.sun == doctest::Approx(1.6f));
    World& w = app.world();
    bool lights_on_circle = true;
    w.each<const Transform, const orbits::Orbit, const PointLight>([&](const Transform& t, const orbits::Orbit& o, const PointLight&) {
        lights_on_circle = lights_on_circle && std::abs(std::hypot(t.position.x, t.position.z) - o.radius) < 1e-3f;
    });
    CHECK(lights_on_circle);
    float turned = 0.0f;
    w.each<const Transform, const orbits::Tumble>([&](const Transform& t, const orbits::Tumble&) { turned = std::abs(t.rotation.w - 1.0f); });
    CHECK(turned > 0.01f);
}

TEST_CASE("night: many local lights, ReSTIR on, almost no sun") {
    App app(showcase_desc());
    ShowcaseGame game({.station = StationId::Night});
    app.run_frames(game, 3);
    const Census c = census(app);
    CHECK(c.point_lights == lighting_showcase::night_light_count);
    CHECK(c.point_lights >= 24);
    CHECK(c.cameras == 1);
    CHECK(c.rect_lights == 0);
    CHECK(c.sun < 0.2f);
    CHECK(game.settings().restir_di);
}

TEST_CASE("number keys switch stations and leave nothing behind") {
    Census fresh{};
    {
        App app(showcase_desc());
        ShowcaseGame game({.station = StationId::Room});
        app.run_frames(game, 3);
        fresh = census(app);
    }
    std::vector<InputScriptEvent> script;
    tap(script, 10, Key::Num2);
    tap(script, 20, Key::Num3);
    tap(script, 30, Key::Num1);
    AppDesc desc = showcase_desc();
    desc.input_script = script;
    App app(desc);
    std::vector<StationId> seen;
    Probed game({.station = StationId::Room}, [&](std::uint64_t frame, Context&, ShowcaseGame& g) {
        if (frame == 15 || frame == 25 || frame == 35) seen.push_back(g.station());
    });
    app.run_frames(game, 40);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(seen == std::vector<StationId>{StationId::Yard, StationId::Night, StationId::Room});
    CHECK(census(app) == fresh);
}

TEST_CASE("R toggles ReSTIR, V walks through all six debug views") {
    std::vector<InputScriptEvent> script;
    tap(script, 5, Key::R);
    for (std::uint64_t i = 0; i < 6; ++i) tap(script, 10 + i * 5, Key::V);
    AppDesc desc = showcase_desc();
    desc.input_script = script;
    App app(desc);
    std::vector<DebugView> views;
    bool restir_after_r = true;
    Probed game({.station = StationId::Night}, [&](std::uint64_t frame, Context&, ShowcaseGame& g) {
        if (frame == 8) restir_after_r = g.settings().restir_di;
        if (frame >= 12 && (frame - 12) % 5 == 0 && views.size() < 6) views.push_back(g.settings().debug_view);
    });
    app.run_frames(game, 45);
    CHECK_FALSE(restir_after_r);
    CHECK(views == std::vector<DebugView>{DebugView::Albedo, DebugView::Normals, DebugView::DiffuseLight, DebugView::Specular,
                                          DebugView::Motion, DebugView::None});
}

TEST_CASE("P turns the path tracer on and freezes the animation so it can accumulate") {
    std::vector<InputScriptEvent> script;
    tap(script, 30, Key::P);
    AppDesc desc = showcase_desc();
    desc.input_script = script;
    App app(desc);
    std::vector<Vec3> at10, at20, at40, at60;
    bool path_tracer = false;
    Probed game({.station = StationId::Yard}, [&](std::uint64_t frame, Context& ctx, ShowcaseGame& g) {
        if (frame == 10) at10 = orbiter_positions(ctx.world);
        if (frame == 20) at20 = orbiter_positions(ctx.world);
        if (frame == 40) at40 = orbiter_positions(ctx.world);
        if (frame == 60) {
            at60 = orbiter_positions(ctx.world);
            path_tracer = g.settings().path_tracer;
        }
    });
    app.run_frames(game, 62);
    CHECK(path_tracer);
    CHECK(moved(at10, at20) > 0.5f);
    CHECK(moved(at40, at60) < 1e-4f);
}

TEST_CASE("station names from the command line; an unknown name means the room") {
    CHECK(lighting_showcase::station_from_name("room") == StationId::Room);
    CHECK(lighting_showcase::station_from_name("yard") == StationId::Yard);
    CHECK(lighting_showcase::station_from_name("night") == StationId::Night);
    CHECK(lighting_showcase::station_from_name("cellar") == StationId::Room);
}

TEST_CASE("the yard behaves the same from loose assets and the package") {
    const auto run = [](bool packed) {
        AppDesc desc = showcase_desc();
        if (packed) {
            desc.pack = ASTRAL_SHOWCASE_TEST_PACK;
            REQUIRE(std::filesystem::is_regular_file(desc.pack));
            desc.asset_root = "no-loose-assets-here";
        }
        App app(desc);
        ShowcaseGame game({.station = StationId::Yard});
        app.run_frames(game, 30);
        CHECK(app.context().scripts->take_errors().empty());
        return orbiter_positions(app.world());
    };
    const auto loose = run(false);
    const auto packed = run(true);
    REQUIRE(loose.size() == 6);
    REQUIRE(packed.size() == 6);
    CHECK(moved(loose, packed) < 1e-4f);
}
