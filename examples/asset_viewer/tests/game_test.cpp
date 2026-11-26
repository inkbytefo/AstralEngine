// asset_viewer, headless tests (TDD): covers everything model_viewer and scene_viewer proved
// plus unified CLI resolution, orbit/fly camera switching, live model/scene stats, scene hot reload,
// external-file viewing, and loose-vs-pack & cook-to-pack parity.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "features/fly_camera/fly_camera.hpp"
#include "features/orbit_camera/orbit_camera.hpp"
#include "game.hpp"
#include "options.hpp"
#include "studio.hpp"

#include <astral/assets/cook.hpp>
#include <astral/assets/pack.hpp>
#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using namespace astral;
using asset_viewer::CameraMode;
using asset_viewer::StationId;
using asset_viewer::ViewerGame;
using asset_viewer::ViewerOptions;
using asset_viewer::ViewerStats;

namespace {

AppDesc viewer_desc(std::string_view root = ASTRAL_GAME_ASSETS, std::string_view pack = {}) {
    AppDesc d{};
    d.headless = true;
    d.asset_root = root;
    d.pack = pack;
    d.hot_reload = false;
    d.fixed_timestep = 1.0f / 60.0f;
    return d;
}

ViewerOptions builtin_options(StationId station, CameraMode camera = CameraMode::Orbit) {
    ViewerOptions o{};
    o.station = station;
    o.file = std::string(asset_viewer::default_file_for(station));
    o.asset_root = ASTRAL_GAME_ASSETS;
    o.fallback_assets = ASTRAL_GAME_ASSETS;
    o.camera_mode = camera;
    return o;
}

struct Census {
    std::size_t primitives, models, prefabs, sdf, rect_lights, point_lights, cameras;
    float sun;
    bool operator==(const Census&) const = default;
};

Census census(App& app) {
    World& w = app.world();
    return {w.count<PrimitiveMesh>(), w.count<ModelRef>(),   w.count<PrefabInstance>(), w.count<Sdf>(),
            w.count<RectLight>(),     w.count<PointLight>(), w.count<Camera>(),         app.context().sun.intensity};
}

void tap(std::vector<InputScriptEvent>& script, std::uint64_t frame, Key key) {
    script.push_back({.frame = frame, .key = key, .down = true});
    script.push_back({.frame = frame + 2, .key = key, .down = false});
}

void hold(std::vector<InputScriptEvent>& script, std::uint64_t start, std::uint64_t end, Key key) {
    script.push_back({.frame = start, .key = key, .down = true});
    script.push_back({.frame = end, .key = key, .down = false});
}

template <class Probe> struct Probed final : Game {
    ViewerGame inner;
    Probe probe;
    Probed(ViewerOptions options, Probe p) : inner(std::move(options)), probe(std::move(p)) {}
    void on_start(Context& ctx) override { inner.on_start(ctx); }
    void on_update(Context& ctx) override {
        inner.on_update(ctx);
        probe(ctx.time.frame, ctx, inner);
    }
};

struct TempDir {
    std::filesystem::path path;
    explicit TempDir(std::string_view tag) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("astral_av_" + std::string(tag) + "_" + std::to_string(stamp));
        std::filesystem::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

Vec3 first_camera_position(World& w) {
    Vec3 pos{};
    w.each<const Transform, const Camera>([&](const Transform& t, const Camera&) { pos = t.position; });
    return pos;
}

} // namespace

TEST_CASE("CLI resolution picks built-in stations, external models/scenes, and flags") {
    const auto parse = [](std::initializer_list<const char*> args) {
        std::vector<const char*> argv{"asset_viewer"};
        for (const char* a : args) argv.push_back(a);
        const ShellOptions shell = parse_shell_args(static_cast<int>(argv.size()), argv.data());
        return asset_viewer::resolve_launch(shell, ASTRAL_GAME_ASSETS, "asset_viewer.apak");
    };

    const ViewerOptions def = parse({});
    CHECK(def.station == StationId::Scene);
    CHECK(def.file == "showcase.scene");
    CHECK_FALSE(def.external);
    CHECK(def.camera_mode == CameraMode::Orbit);

    const ViewerOptions model = parse({"--view", "model", "--camera", "fly", "--debug", "--ui", "--raster"});
    CHECK(model.station == StationId::Model);
    CHECK(model.file == "props/crate.glb");
    CHECK_FALSE(model.external);
    CHECK(model.camera_mode == CameraMode::Fly);
    CHECK(model.debug);
    CHECK(model.ui);
    CHECK(model.raster);

    const ViewerOptions unknown = parse({"--view", "mystery"});
    CHECK(unknown.station == StationId::Scene);
    CHECK(unknown.file == "showcase.scene");

    const std::filesystem::path crate_abs = std::filesystem::path(ASTRAL_GAME_ASSETS) / "props" / "crate.glb";
    const std::string crate_str = crate_abs.string();
    const ViewerOptions ext_model = parse({crate_str.c_str()});
    CHECK(ext_model.station == StationId::Model);
    CHECK(ext_model.file == "crate.glb");
    CHECK(ext_model.external);
    CHECK(ext_model.pack.empty());

    const std::filesystem::path scene_abs = std::filesystem::path(ASTRAL_GAME_ASSETS) / "showcase.scene";
    const std::string scene_str = scene_abs.string();
    const ViewerOptions ext_scene = parse({scene_str.c_str(), "--debug"});
    CHECK(ext_scene.station == StationId::Scene);
    CHECK(ext_scene.file == "showcase.scene");
    CHECK(ext_scene.external);
    CHECK(ext_scene.debug);

    CHECK(asset_viewer::is_model_extension("robot.glb"));
    CHECK(asset_viewer::is_model_extension("ROBOT.GLTF"));
    CHECK_FALSE(asset_viewer::is_model_extension("showcase.scene"));
}

TEST_CASE("scene station: loads RGB room with prefabs, SDF shapes, model ref and stats") {
    App app(viewer_desc());
    ViewerGame game(builtin_options(StationId::Scene));
    app.run_frames(game, 5);
    CHECK_FALSE(app.context().quit_requested());
    CHECK(app.context().scripts->take_errors().empty());
    const Census c = census(app);
    CHECK(c.cameras == 1);
    CHECK(c.primitives >= 5);
    CHECK(c.prefabs >= 5);
    CHECK(c.sdf >= 3);
    CHECK(c.models == 1);
    CHECK(c.rect_lights + c.point_lights >= 1);
    CHECK(c.sun == doctest::Approx(0.0f));
    const ViewerStats& s = game.stats();
    CHECK(s.entities >= 15);
    CHECK(s.prefab_instances >= 5);
    CHECK(s.model.loaded_models == 1);
    CHECK(s.model.vertices > 0);
    CHECK(s.model.triangles > 0);
    CHECK(s.model.nodes >= 1);
    CHECK_FALSE(s.from_pack);
}

TEST_CASE("model station: builds the lit studio around a glTF/GLB model and inspects its stats") {
    App app(viewer_desc());
    ViewerGame game(builtin_options(StationId::Model));
    app.run_frames(game, 5);
    CHECK_FALSE(app.context().quit_requested());
    CHECK(app.context().scripts->take_errors().empty());
    const Census c = census(app);
    CHECK(c.models == 1);
    CHECK(c.rect_lights == 1);
    CHECK(c.cameras == 1);
    CHECK(c.sun == doctest::Approx(1.2f));
    const GiSettings gi = game.settings().gi;
    CHECK(gi.volume_min.x == doctest::Approx(-4.0f));
    CHECK(gi.volume_max.z == doctest::Approx(4.0f));
    const ViewerStats& s = game.stats();
    CHECK(s.model.loaded_models == 1);
    CHECK(s.model.vertices > 0);
    CHECK(s.model.triangles > 0);
    CHECK(s.model.materials >= 1);
}

TEST_CASE("flags: --debug draws overlays, --ui opens debug UI, --raster adds stand-in light") {
    ViewerOptions opts = builtin_options(StationId::Scene);
    opts.debug = true;
    opts.ui = true;
    opts.raster = true;
    App app(viewer_desc());
    std::size_t debug_lines = 0;
    Probed game(opts, [&](std::uint64_t frame, Context& ctx, ViewerGame&) {
        if (frame == 2) debug_lines = ctx.debug.lines().size();
    });
    app.run_frames(game, 4);
    CHECK(app.context().debug_show.lights);
    CHECK(app.context().debug_show.physics);
    CHECK(app.context().show_debug_ui);
    CHECK(debug_lines > 0);
    CHECK_FALSE(game.inner.settings().ray_tracing);
    CHECK(app.world().count<PointLight>() >= 1);
}

TEST_CASE("number keys 1/2 switch between scene and model stations without leftovers") {
    Census fresh_scene{};
    {
        App app(viewer_desc());
        ViewerGame game(builtin_options(StationId::Scene));
        app.run_frames(game, 4);
        fresh_scene = census(app);
    }
    std::vector<InputScriptEvent> script;
    tap(script, 8, Key::Num2);
    tap(script, 18, Key::Num1);
    AppDesc desc = viewer_desc();
    desc.input_script = script;
    App app(desc);
    StationId mid_station = StationId::Scene;
    Probed game(builtin_options(StationId::Scene), [&](std::uint64_t frame, Context&, ViewerGame& g) {
        if (frame == 14) mid_station = g.station();
    });
    app.run_frames(game, 25);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(mid_station == StationId::Model);
    CHECK(game.inner.station() == StationId::Scene);
    CHECK(census(app) == fresh_scene);
}

TEST_CASE("orbit camera orbits with arrows, zooms with +/-, resets with R, and toggles to fly with C") {
    std::vector<InputScriptEvent> script;
    hold(script, 4, 24, Key::Right);  // orbit around target
    hold(script, 26, 40, Key::Minus); // zoom out
    tap(script, 44, Key::R);          // reset to home pose
    tap(script, 50, Key::C);          // switch to Fly mode
    hold(script, 54, 70, Key::W);     // fly forward
    tap(script, 74, Key::G);          // toggle debug overlays on

    AppDesc desc = viewer_desc();
    desc.input_script = script;
    App app(desc);

    Vec3 p_home{}, p_orbited{}, p_zoomed{}, p_reset{}, p_fly_start{}, p_fly_end{};
    CameraMode mode_initial = CameraMode::Fly;
    CameraMode mode_after_c = CameraMode::Orbit;
    bool has_orbit_initial = false, has_fly_after_c = false;
    Probed game(builtin_options(StationId::Model), [&](std::uint64_t frame, Context& ctx, ViewerGame& g) {
        if (frame == 2) {
            p_home = first_camera_position(ctx.world);
            mode_initial = g.camera_mode();
            has_orbit_initial = ctx.world.count<orbit::OrbitCamera>() == 1;
        }
        if (frame == 25) p_orbited = first_camera_position(ctx.world);
        if (frame == 41) p_zoomed = first_camera_position(ctx.world);
        if (frame == 47) p_reset = first_camera_position(ctx.world);
        if (frame == 53) {
            p_fly_start = first_camera_position(ctx.world);
            mode_after_c = g.camera_mode();
            has_fly_after_c = ctx.world.count<fly::FlyCamera>() == 1 && ctx.world.count<orbit::OrbitCamera>() == 0;
        }
        if (frame == 71) p_fly_end = first_camera_position(ctx.world);
    });
    app.run_frames(game, 78);

    const asset_viewer::Studio studio;
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(mode_initial == CameraMode::Orbit);
    CHECK(has_orbit_initial);
    CHECK(length(p_orbited - p_home) > 0.3f);
    CHECK(length(p_orbited - studio.target) == doctest::Approx(length(p_home - studio.target)).epsilon(0.05f));
    CHECK(length(p_zoomed - studio.target) > length(p_orbited - studio.target) + 0.3f);
    CHECK(length(p_reset - p_home) < 0.02f);
    CHECK(mode_after_c == CameraMode::Fly);
    CHECK(has_fly_after_c);
    CHECK(length(p_fly_start - p_reset) < 0.02f);
    CHECK(length(p_fly_end - p_fly_start) > 0.3f);
    CHECK(game.inner.debug_overlays());
}

TEST_CASE("external model file loads in the studio with working viewer features") {
    TempDir tmp("extmodel");
    const std::filesystem::path src_glb = std::filesystem::path(ASTRAL_GAME_ASSETS) / "props" / "crate.glb";
    const std::filesystem::path dst_glb = tmp.path / "crate.glb";
    std::filesystem::copy_file(src_glb, dst_glb);

    const std::string glb_arg = dst_glb.string();
    const char* argv[] = {"asset_viewer", glb_arg.c_str()};
    const ShellOptions shell = parse_shell_args(2, argv);
    const ViewerOptions opts = asset_viewer::resolve_launch(shell, ASTRAL_GAME_ASSETS, "asset_viewer.apak");
    REQUIRE(opts.external);
    REQUIRE(opts.station == StationId::Model);

    std::vector<InputScriptEvent> script;
    hold(script, 4, 20, Key::Right);
    AppDesc desc = viewer_desc(opts.asset_root, opts.pack);
    desc.input_script = script;
    App app(desc);
    Vec3 p0{}, p1{};
    Probed game(opts, [&](std::uint64_t frame, Context& ctx, ViewerGame&) {
        if (frame == 2) p0 = first_camera_position(ctx.world);
        if (frame == 22) p1 = first_camera_position(ctx.world);
    });
    app.run_frames(game, 24);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(app.world().count<ModelRef>() == 1);
    CHECK(app.world().count<RectLight>() == 1);
    CHECK(game.inner.stats().model.loaded_models == 1);
    CHECK(game.inner.stats().model.triangles > 0);
    CHECK(length(p1 - p0) > 0.2f);
}

TEST_CASE("scene hot reload refreshes entities when the .scene document changes on disk") {
    TempDir tmp("hotreload");
    const std::filesystem::path scene_file = tmp.path / "live.scene";
    {
        std::ofstream out(scene_file, std::ios::binary);
        out << R"({
  "format": "astral.scene",
  "version": 1,
  "entities": [
    { "id": 1, "components": { "astral::Name": { "value": "box_a" }, "astral::Transform": {}, "astral::PrimitiveMesh": { "shape": "Cube" } } },
    { "id": 2, "components": { "astral::Name": { "value": "cam" }, "astral::Transform": { "position": [0, 2, 6] }, "astral::Camera": {} } }
  ]
})";
    }

    ViewerOptions opts{};
    opts.station = StationId::Scene;
    opts.file = "live.scene";
    opts.asset_root = tmp.path.string();
    opts.fallback_assets = ASTRAL_GAME_ASSETS;
    opts.external = true;

    AppDesc desc = viewer_desc(opts.asset_root);
    desc.hot_reload = true;
    App app(desc);
    std::size_t before_prims = 0, after_prims = 0;
    bool edited = false;
    Probed game(opts, [&](std::uint64_t frame, Context& ctx, ViewerGame&) {
        if (frame == 2) {
            before_prims = ctx.world.count<PrimitiveMesh>();
            {
                std::ofstream out(scene_file, std::ios::binary | std::ios::trunc);
                out << R"({
  "format": "astral.scene",
  "version": 1,
  "entities": [
    { "id": 1, "components": { "astral::Name": { "value": "box_a" }, "astral::Transform": {}, "astral::PrimitiveMesh": { "shape": "Cube" } } },
    { "id": 2, "components": { "astral::Name": { "value": "box_b" }, "astral::Transform": { "position": [2, 0, 0] }, "astral::PrimitiveMesh": { "shape": "Sphere" } } },
    { "id": 3, "components": { "astral::Name": { "value": "cam" }, "astral::Transform": { "position": [0, 2, 6] }, "astral::Camera": {} } }
  ]
})";
            }
            const auto now_time = std::filesystem::last_write_time(scene_file);
            std::filesystem::last_write_time(scene_file, now_time + std::chrono::seconds(2));
            std::this_thread::sleep_for(std::chrono::milliseconds(550));
            edited = true;
        }
        if (frame == 6) after_prims = ctx.world.count<PrimitiveMesh>();
    });
    app.run_frames(game, 8);
    CHECK(edited);
    CHECK(before_prims == 1);
    CHECK(after_prims == 2);
    CHECK(game.inner.scene_reloads() >= 1);
}

TEST_CASE("scene and model stations behave the same from loose assets and the package") {
    const auto run = [](StationId station, bool packed) {
        AppDesc desc = viewer_desc();
        ViewerOptions opts = builtin_options(station);
        if (packed) {
            desc.pack = ASTRAL_SHOWCASE_TEST_PACK;
            REQUIRE(std::filesystem::is_regular_file(desc.pack));
            desc.asset_root = "no-loose-assets-here";
            opts.asset_root = "no-loose-assets-here";
            opts.pack = ASTRAL_SHOWCASE_TEST_PACK;
        }
        App app(desc);
        ViewerGame game(opts);
        app.run_frames(game, 6);
        CHECK(app.context().scripts->take_errors().empty());
        CHECK(game.stats().from_pack == packed);
        return std::pair{census(app), game.stats().model};
    };

    const auto [scene_loose, scene_model_loose] = run(StationId::Scene, false);
    const auto [scene_packed, scene_model_packed] = run(StationId::Scene, true);
    CHECK(scene_loose == scene_packed);
    CHECK(scene_model_loose == scene_model_packed);
    CHECK(scene_model_packed.vertices > 0);
    CHECK(scene_model_packed.triangles > 0);

    const auto [model_loose, model_stats_loose] = run(StationId::Model, false);
    const auto [model_packed, model_stats_packed] = run(StationId::Model, true);
    CHECK(model_loose == model_packed);
    CHECK(model_stats_loose == model_stats_packed);
    CHECK(model_stats_packed.triangles > 0);
}

TEST_CASE("cook and package round-trip opens cleanly in asset_viewer") {
    TempDir tmp("cookpack");
    const std::filesystem::path src_dir = tmp.path / "src";
    const std::filesystem::path cooked_dir = tmp.path / "cooked";
    const std::filesystem::path pack_file = tmp.path / "custom.apak";
    std::filesystem::create_directories(src_dir / "props");
    std::filesystem::copy_file(std::filesystem::path(ASTRAL_GAME_ASSETS) / "props" / "crate.glb",
                               src_dir / "props" / "crate.glb");
    std::filesystem::copy(std::filesystem::path(ASTRAL_GAME_ASSETS) / "features", src_dir / "features",
                          std::filesystem::copy_options::recursive);

    const CookReport report = cook_directory({.source = src_dir, .output = cooked_dir, .textures = {.compress = false}});
    REQUIRE(report.ok());
    std::string pack_error;
    REQUIRE(write_pack(cooked_dir, pack_file, &pack_error));

    ViewerOptions opts = builtin_options(StationId::Model);
    opts.asset_root = "no-loose-assets-here";
    opts.pack = pack_file.string();

    App app(viewer_desc(opts.asset_root, opts.pack));
    ViewerGame game(opts);
    app.run_frames(game, 6);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(game.stats().from_pack);
    CHECK(game.stats().model.loaded_models == 1);
    CHECK(game.stats().model.triangles > 0);
}
