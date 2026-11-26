// materials_showcase, headless tests: covers studio (7 M13 PBR models + car_paint.ashader + 2 decals + P toggle),
// workflow (.material inheritance, triplanar, GLB slot override, U UV/density toggle), gallery (.ashader night gallery),
// clean station switching (including ctx.sky and ctx.post), all .ashader compiles + RT variants, all .material resolves,
// and loose-vs-pack parity.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "features/fly_camera/fly_camera.hpp"
#include "game.hpp"
#include "stations/stations.hpp"

#include <astral/render/material_file.hpp>
#include <astral/render/surface_shader.hpp>
#include <astral/world/scene.hpp>

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

using namespace astral;
using materials_showcase::Options;
using materials_showcase::ShowcaseGame;
using materials_showcase::StationId;
using materials_showcase::UvInspectMode;

namespace {

AppDesc showcase_desc() {
    AppDesc d{};
    d.headless = true;
    d.asset_root = ASTRAL_GAME_ASSETS;
    d.hot_reload = false;
    d.fixed_timestep = 1.0f / 60.0f;
    return d;
}

/// Snapshot of everything a station puts in the World and global Context.
struct Census {
    std::size_t meshes, primitives, models, parents, decals, rect_lights, point_lights, cameras, fly_cameras;
    float sun, ambient, exposure;
    bool has_sky;
    std::size_t post_count;
    bool operator==(const Census&) const = default;
};

Census census(App& app) {
    World& w = app.world();
    const Context& ctx = app.context();
    return {w.count<MeshRenderer>(), w.count<PrimitiveMesh>(), w.count<ModelRef>(),   w.count<Parent>(),
            w.count<Decal>(),        w.count<RectLight>(),     w.count<PointLight>(), w.count<Camera>(),
            w.count<fly::FlyCamera>(), ctx.sun.intensity,      ctx.environment.ambient, ctx.environment.exposure,
            ctx.sky.valid(),         ctx.post.size()};
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

std::string read_asset_text(std::string_view relative_path) {
    const auto path = std::filesystem::path(ASTRAL_GAME_ASSETS) / relative_path;
    std::ifstream file(path, std::ios::binary);
    REQUIRE_MESSAGE(file.is_open(), "cannot open ", path.string());
    return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

MaterialReader disk_material_reader() {
    return [](std::string_view path, std::string* error) -> std::optional<Json> {
        const auto full = std::filesystem::path(ASTRAL_GAME_ASSETS) / path;
        std::ifstream file(full, std::ios::binary);
        if (!file.is_open()) {
            if (error) *error = "cannot open " + full.string();
            return std::nullopt;
        }
        const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        return Json::parse(text, error);
    };
}

} // namespace

TEST_CASE("studio: spawns 7 PBR spheres, car_paint.ashader exhibit, 2 decals, lights and camera") {
    App app(showcase_desc());
    ShowcaseGame game({.station = StationId::Studio});
    app.run_frames(game, 3);
    CHECK_FALSE(app.context().quit_requested());
    CHECK(app.context().scripts->take_errors().empty());
    const Census c = census(app);
    // Floor + wall + 8 plinths + 8 spheres = 18 rendered meshes (7 PBR spheres + 1 car_paint PrimitiveMesh).
    CHECK(c.meshes == 18u);
    CHECK(c.primitives == 1u);
    CHECK(c.decals == 2u);
    CHECK(c.rect_lights == 1u);
    CHECK(c.point_lights == 1u);
    CHECK(c.cameras == 1u);
    CHECK(c.fly_cameras == 1u);
    CHECK(c.sun == doctest::Approx(1.6f));
    CHECK(c.ambient == doctest::Approx(0.35f));
    CHECK_FALSE(c.has_sky);
    CHECK(c.post_count == 0u);

    const Entity car_paint = find_by_name(app.world(), "car_paint_exhibit");
    REQUIRE(car_paint.valid());
    const PrimitiveMesh* mesh = app.world().get<PrimitiveMesh>(car_paint);
    REQUIRE(mesh != nullptr);
    CHECK(mesh->shape == Primitive::Sphere);
    CHECK(mesh->material_file == materials_showcase::studio_car_paint_material);
}

TEST_CASE("studio: raster fallback adds stand-in point light and P toggles path tracer only when RT enabled") {
    {
        std::vector<InputScriptEvent> script;
        tap(script, 5, Key::P);
        tap(script, 15, Key::P);
        AppDesc desc = showcase_desc();
        desc.input_script = script;
        App app(desc);
        bool after_first = false, after_second = true;
        Probed game({.station = StationId::Studio}, [&](std::uint64_t frame, Context&, ShowcaseGame& g) {
            if (frame == 8) after_first = g.settings().path_tracer;
            if (frame == 18) after_second = g.settings().path_tracer;
        });
        app.run_frames(game, 22);
        CHECK(after_first);
        CHECK_FALSE(after_second);
    }
    {
        std::vector<InputScriptEvent> script;
        tap(script, 5, Key::P);
        AppDesc desc = showcase_desc();
        desc.input_script = script;
        App app(desc);
        ShowcaseGame game({.station = StationId::Studio, .raster = true});
        app.run_frames(game, 10);
        CHECK_FALSE(game.settings().ray_tracing);
        CHECK_FALSE(game.settings().path_tracer);
        CHECK(census(app).point_lights == 2u); // rear SSS light + overhead raster stand-in
    }
}

TEST_CASE("workflow: loads scene, resolves GLB models, slot override and parent-inherited materials") {
    App app(showcase_desc());
    ShowcaseGame game({.station = StationId::Workflow});
    app.run_frames(game, 3);
    CHECK_FALSE(app.context().quit_requested());
    CHECK(app.context().scripts->take_errors().empty());
    World& w = app.world();
    CHECK(w.count<ModelRef>() == 4u);
    CHECK(w.count<Camera>() == 1u);
    CHECK(w.count<fly::FlyCamera>() == 1u);
    CHECK(w.count<MeshRenderer>() > 10u);

    const Entity blue_door = find_by_name(w, "Blue override");
    REQUIRE(blue_door.valid());
    const ModelRef* door_ref = w.get<ModelRef>(blue_door);
    REQUIRE(door_ref != nullptr);
    CHECK(door_ref->path == "workflow/models/door.glb");
    REQUIRE(door_ref->material_overrides.size() == 1u);
    CHECK(door_ref->material_overrides[0].slot == "oak");
    CHECK(door_ref->material_overrides[0].material_file == "workflow/materials/blue.material");

    const Entity triplanar = find_by_name(w, "Stone sphere (triplanar, parent: stone)");
    const Entity wet = find_by_name(w, "Wet stone sphere (parent: triplanar)");
    REQUIRE(triplanar.valid());
    REQUIRE(wet.valid());
    CHECK(w.get<PrimitiveMesh>(triplanar)->material_file == "workflow/materials/stone_triplanar.material");
    CHECK(w.get<PrimitiveMesh>(wet)->material_file == "workflow/materials/stone_wet.material");
}

TEST_CASE("workflow: U cycles uv_inspect checker, texel density, and restores original materials") {
    std::vector<InputScriptEvent> script;
    tap(script, 5, Key::U);
    tap(script, 12, Key::U);
    tap(script, 19, Key::U);
    AppDesc desc = showcase_desc();
    desc.input_script = script;
    App app(desc);

    std::vector<UvInspectMode> modes;
    std::vector<std::string> wall_mats;
    std::vector<std::string> blue_door_mats;
    std::size_t oak_overrides_off = 99, oak_overrides_checker = 0;

    Probed game({.station = StationId::Workflow}, [&](std::uint64_t frame, Context& ctx, ShowcaseGame& g) {
        if (frame == 3 || frame == 8 || frame == 15 || frame == 22) {
            modes.push_back(g.uv_mode());
            const Entity wall = find_by_name(ctx.world, "Textured stone wall");
            const Entity blue = find_by_name(ctx.world, "Blue override");
            const Entity oak = find_by_name(ctx.world, "Imported oak");
            if (const auto* pm = ctx.world.get<PrimitiveMesh>(wall)) wall_mats.push_back(pm->material_file);
            if (const auto* mr = ctx.world.get<ModelRef>(blue); mr && !mr->material_overrides.empty()) {
                blue_door_mats.push_back(mr->material_overrides[0].material_file);
            }
            if (const auto* mr = ctx.world.get<ModelRef>(oak)) {
                if (frame == 8) oak_overrides_checker = mr->material_overrides.size();
                if (frame == 22) oak_overrides_off = mr->material_overrides.size();
            }
        }
    });
    app.run_frames(game, 25);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(modes == std::vector<UvInspectMode>{UvInspectMode::Off, UvInspectMode::Checker, UvInspectMode::Density, UvInspectMode::Off});
    CHECK(wall_mats == std::vector<std::string>{"workflow/materials/stone.material", "workflow/materials/uv_inspect.material",
                                                "workflow/materials/uv_density.material", "workflow/materials/stone.material"});
    CHECK(blue_door_mats == std::vector<std::string>{"workflow/materials/blue.material", "workflow/materials/uv_inspect.material",
                                                     "workflow/materials/uv_density.material", "workflow/materials/blue.material"});
    CHECK(oak_overrides_checker > 0u);
    CHECK(oak_overrides_off == 0u);
}

TEST_CASE("gallery: loads every night gallery exhibit, lights, bloom and fly camera") {
    App app(showcase_desc());
    ShowcaseGame game({.station = StationId::Gallery});
    app.run_frames(game, 3);
    Context& ctx = app.context();
    World& w = ctx.world;
    CHECK_FALSE(ctx.quit_requested());
    CHECK(ctx.scripts->take_errors().empty());

    struct Exhibit {
        std::string_view name;
        Primitive shape;
        std::string_view material;
    };
    constexpr std::array exhibits{
        Exhibit{"leaf_1", Primitive::Plane, "gallery/materials/leaf.material"},
        Exhibit{"leaf_7", Primitive::Plane, "gallery/materials/leaf.material"},
        Exhibit{"glass", Primitive::Sphere, "gallery/materials/glass.material"},
        Exhibit{"blue_glass", Primitive::Sphere, "gallery/materials/glass_blue.material"},
        Exhibit{"lava", Primitive::Sphere, "gallery/materials/lava.material"},
        Exhibit{"alloy", Primitive::Sphere, "gallery/materials/alloy.material"},
        Exhibit{"hologram", Primitive::Cube, "gallery/materials/hologram.material"},
        Exhibit{"plasma_shell", Primitive::Sphere, "gallery/materials/plasma_shell.material"},
        Exhibit{"floor", Primitive::Plane, "gallery/materials/gallery_floor.material"},
    };
    CHECK(w.count<PrimitiveMesh>() == 31u);
    for (const Exhibit& ex : exhibits) {
        CAPTURE(ex.name);
        const PrimitiveMesh* mesh = w.get<PrimitiveMesh>(find_by_name(w, ex.name));
        REQUIRE(mesh != nullptr);
        CHECK(mesh->shape == ex.shape);
        CHECK(mesh->material_file == ex.material);
    }
    CHECK(w.count<PointLight>() == 5u);
    const PointLight* lava_light = w.get<PointLight>(find_by_name(w, "lava_light"));
    REQUIRE(lava_light != nullptr);
    CHECK(lava_light->intensity == doctest::Approx(20.0f));
    CHECK(ctx.sun.intensity == doctest::Approx(0.95f));
    CHECK(ctx.environment.exposure == doctest::Approx(0.86f));
    CHECK(game.settings().bloom_strength == doctest::Approx(0.12f));
    CHECK(w.count<Camera>() == 1u);
    CHECK(w.count<fly::FlyCamera>() == 1u);
}

TEST_CASE("station switching via 1/2/3 leaves nothing behind, including ctx.sky, ctx.post and model pieces") {
    Census fresh_studio{};
    Census fresh_workflow{};
    {
        App app(showcase_desc());
        ShowcaseGame g({.station = StationId::Studio});
        app.run_frames(g, 3);
        fresh_studio = census(app);
    }
    {
        App app(showcase_desc());
        ShowcaseGame g({.station = StationId::Workflow});
        app.run_frames(g, 3);
        fresh_workflow = census(app);
    }

    std::vector<InputScriptEvent> script;
    tap(script, 8, Key::Num2);  // -> Workflow
    tap(script, 14, Key::U);    // enable UV inspect on Workflow
    tap(script, 20, Key::Num3); // -> Gallery (must drop Workflow models + reset UV mode)
    tap(script, 30, Key::Num1); // -> Studio (must clear ctx.sky and ctx.post)
    tap(script, 40, Key::Num3); // -> Gallery
    tap(script, 50, Key::Num2); // -> Workflow (must clear ctx.sky and ctx.post)
    AppDesc desc = showcase_desc();
    desc.input_script = script;
    App app(desc);

    std::vector<StationId> seen;
    bool sky_cleared_for_studio = false, post_cleared_for_studio = false;
    bool sky_cleared_for_workflow = false, post_cleared_for_workflow = false;
    UvInspectMode uv_on_gallery = UvInspectMode::Density;
    Census studio_after_roundtrip{};

    Probed game({.station = StationId::Studio}, [&](std::uint64_t frame, Context& ctx, ShowcaseGame& g) {
        if (frame == 12 || frame == 25 || frame == 35 || frame == 55) seen.push_back(g.station());
        // Simulate GPU-loaded sky/post handles while in Gallery to prove switching away clears them.
        if (frame == 25) {
            uv_on_gallery = g.uv_mode();
            ctx.sky = MaterialHandle{1, 1};
            ctx.post.push_back(MaterialHandle{2, 1});
        }
        if (frame == 35) {
            sky_cleared_for_studio = !ctx.sky.valid();
            post_cleared_for_studio = ctx.post.empty();
            studio_after_roundtrip = census(app);
        }
        if (frame == 45) {
            ctx.sky = MaterialHandle{3, 1};
            ctx.post.push_back(MaterialHandle{4, 1});
        }
        if (frame == 55) {
            sky_cleared_for_workflow = !ctx.sky.valid();
            post_cleared_for_workflow = ctx.post.empty();
        }
    });
    app.run_frames(game, 58);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(seen == std::vector<StationId>{StationId::Workflow, StationId::Gallery, StationId::Studio, StationId::Workflow});
    CHECK(uv_on_gallery == UvInspectMode::Off);
    CHECK(sky_cleared_for_studio);
    CHECK(post_cleared_for_studio);
    CHECK(sky_cleared_for_workflow);
    CHECK(post_cleared_for_workflow);
    CHECK(studio_after_roundtrip == fresh_studio);
    CHECK(census(app) == fresh_workflow);
}

TEST_CASE("all .ashader files compile for their draw paths and together for ray tracing") {
    constexpr std::array shaders{
        "studio/shaders/car_paint.ashader",
        "workflow/shaders/uv_inspect.ashader",
        "gallery/shaders/foliage.ashader",
        "gallery/shaders/glass.ashader",
        "gallery/shaders/lava.ashader",
        "gallery/shaders/hologram.ashader",
        "gallery/shaders/dusk_sky.ashader",
        "gallery/shaders/vignette.ashader",
        "gallery/shaders/alloy.ashader",
        "gallery/shaders/gallery_floor.ashader",
        "gallery/shaders/plasma_shell.ashader",
    };
    std::vector<RtSurface> rt_surfaces;
    for (const std::string_view rel : shaders) {
        CAPTURE(rel);
        const std::string source = read_asset_text(rel);
        const ShaderParse parsed = parse_ashader(source);
        for (const auto& error : parsed.errors) FAIL_CHECK(error.line, ": ", error.message);
        REQUIRE(parsed.errors.empty());

        if (parsed.source.type == ShaderType::Surface) {
            auto shader = compile_surface_shader(parsed.source, rel);
            for (const auto& error : shader.errors) FAIL_CHECK(error.line, ": ", error.message);
            REQUIRE(shader.ok());
            if (!shader.rt_chunk.empty()) {
                rt_surfaces.push_back({static_cast<std::uint32_t>(rt_surfaces.size()), std::move(shader.rt_chunk), shader.alpha_test});
            }
        } else {
            const auto shader = compile_screen_shader(parsed.source, rel);
            for (const auto& error : shader.errors) FAIL_CHECK(error.line, ": ", error.message);
            REQUIRE(shader.ok());
        }
    }
    REQUIRE(rt_surfaces.size() == 6u);
    const auto rt = compile_rt_variants(rt_surfaces);
    INFO(rt.log);
    CHECK(rt.ok());
}

TEST_CASE("all .material files resolve cleanly and parent inheritance works") {
    const MaterialReader reader = disk_material_reader();
    constexpr std::array materials{
        "studio/materials/car_paint.material",
        "workflow/materials/blue.material",
        "workflow/materials/floor.material",
        "workflow/materials/stone.material",
        "workflow/materials/stone_triplanar.material",
        "workflow/materials/stone_wet.material",
        "workflow/materials/uv_inspect.material",
        "workflow/materials/uv_density.material",
        "gallery/materials/alloy.material",
        "gallery/materials/dusk_sky.material",
        "gallery/materials/gallery_floor.material",
        "gallery/materials/glass.material",
        "gallery/materials/glass_blue.material",
        "gallery/materials/hologram.material",
        "gallery/materials/lava.material",
        "gallery/materials/leaf.material",
        "gallery/materials/plasma_shell.material",
        "gallery/materials/vignette.material",
    };
    for (const std::string_view rel : materials) {
        CAPTURE(rel);
        const MaterialFile mf = resolve_material(rel, reader);
        for (const auto& e : mf.errors) FAIL_CHECK(e);
        REQUIRE(mf.ok());
        if (!mf.standard_pbr) {
            const ShaderParse parsed = parse_ashader(read_asset_text(mf.shader));
            REQUIRE(parsed.errors.empty());
            const UniformValues uv = evaluate_uniforms(parsed.source, layout_uniforms(parsed.source), mf);
            for (const auto& w : uv.warnings) FAIL_CHECK(w);
            CHECK(uv.warnings.empty());
        }
    }

    const MaterialFile wet = resolve_material("workflow/materials/stone_wet.material", reader);
    REQUIRE(wet.ok());
    CHECK(wet.standard_pbr);
    CHECK(wet.dependencies.size() == 3u);
    REQUIRE(wet.value("base_color_texture") != nullptr);
    CHECK(wet.value("base_color_texture")->as_string() == "workflow/textures/stone.png");
    REQUIRE(wet.value("mapping") != nullptr);
    CHECK((*wet.value("mapping"))["projection"].as_string() == "Triplanar");
    REQUIRE(wet.value("wetness") != nullptr);
    CHECK(wet.value("wetness")->as_number() == doctest::Approx(0.85));

    const MaterialFile density = resolve_material("workflow/materials/uv_density.material", reader);
    REQUIRE(density.ok());
    CHECK(density.shader == "workflow/shaders/uv_inspect.ashader");
    REQUIRE(density.value("show_density") != nullptr);
    CHECK(density.value("show_density")->as_bool());
}

TEST_CASE("station names from the command line; unknown name falls back to studio") {
    CHECK(materials_showcase::station_from_name("studio") == StationId::Studio);
    CHECK(materials_showcase::station_from_name("workflow") == StationId::Workflow);
    CHECK(materials_showcase::station_from_name("gallery") == StationId::Gallery);
    CHECK(materials_showcase::station_from_name("unknown") == StationId::Studio);
}

TEST_CASE("workflow and gallery behave the same from loose assets and the package") {
    const auto run = [](StationId station, bool packed) {
        AppDesc desc = showcase_desc();
        if (packed) {
            desc.pack = ASTRAL_SHOWCASE_TEST_PACK;
            REQUIRE(std::filesystem::is_regular_file(desc.pack));
            desc.asset_root = "no-loose-assets-here";
        }
        App app(desc);
        ShowcaseGame game({.station = station});
        app.run_frames(game, 3);
        CHECK_FALSE(app.context().quit_requested());
        CHECK(app.context().scripts->take_errors().empty());
        return census(app);
    };
    const Census wf_loose = run(StationId::Workflow, false);
    const Census wf_packed = run(StationId::Workflow, true);
    CHECK(wf_loose.models == 4u);
    CHECK(wf_loose == wf_packed);

    const Census gal_loose = run(StationId::Gallery, false);
    const Census gal_packed = run(StationId::Gallery, true);
    CHECK(gal_loose.primitives == 31u);
    CHECK(gal_loose == gal_packed);
}
