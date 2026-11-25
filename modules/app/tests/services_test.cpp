// Focused headless unit tests for each internal App service (M13.5 R2).
#include "../../assets/tests/gltf_writer.hpp"

#include "../src/services/audio_bridge.hpp"
#include "../src/services/dev_tools.hpp"
#include "../src/services/physics_bridge.hpp"
#include "../src/services/render_bridge.hpp"
#include "../src/services/script_bridge.hpp"
#include "../src/services/world_bridge.hpp"

#include <astral/assets/asset_manager.hpp>
#include <astral/core/jobs.hpp>
#include <astral/core/log.hpp>
#include <astral/core/profiler.hpp>
#include <astral/platform/input.hpp>
#include <astral/render/text.hpp>
#include <astral/world/transform.hpp>
#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

using namespace astral;

namespace {

void write_text(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    test::write_bytes(file, std::vector<std::uint8_t>(text.begin(), text.end()));
}

std::vector<std::uint8_t> tone_wav(float seconds) {
    const std::uint32_t rate = 48000;
    const auto frames = static_cast<std::uint32_t>(seconds * 48000.0f);
    std::vector<std::uint8_t> out;
    auto u32 = [&](std::uint32_t v) {
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    };
    auto u16 = [&](std::uint16_t v) {
        for (int i = 0; i < 2; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    };
    auto tag = [&](const char* t) { out.insert(out.end(), t, t + 4); };
    tag("RIFF");
    u32(36 + frames * 2);
    tag("WAVE");
    tag("fmt ");
    u32(16);
    u16(1);
    u16(1);
    u32(rate);
    u32(rate * 2);
    u16(2);
    u16(16);
    tag("data");
    u32(frames * 2);
    for (std::uint32_t i = 0; i < frames; ++i) {
        const auto sample = static_cast<std::int16_t>(std::sin(static_cast<float>(i) * 0.06f) * 16000.0f);
        u16(static_cast<std::uint16_t>(sample));
    }
    return out;
}

} // namespace

TEST_CASE("AudioBridge caches sounds and syncs AudioSource voices headlessly") {
    const auto dir = test::temp_dir("service_audio_bridge");
    std::filesystem::create_directories(dir / "sounds");
    test::write_bytes(dir / "sounds" / "loop.wav", tone_wav(0.5f));

    Logger log;
    JobSystem jobs(1);
    World world;
    AssetManager assets({.root = dir, .jobs = &jobs, .logger = &log, .hot_reload = false});
    AudioBridge bridge(world, assets, log, false);

    REQUIRE(bridge.engine() != nullptr);
    const SoundHandle s1 = bridge.sound("sounds/loop.wav");
    const SoundHandle s2 = bridge.sound("sounds/loop.wav");
    CHECK(s1.valid());
    CHECK(s1 == s2);
    CHECK_FALSE(bridge.sound("sounds/missing.wav").valid());

    const Entity src = world.create(Transform{}, AudioSource{.sound = "sounds/loop.wav", .loop = true});
    update_transforms(world);
    bridge.sync();
    CHECK(bridge.engine()->voice_count() == 1);

    world.remove<AudioSource>(src);
    bridge.sync();
    std::vector<float> mix(static_cast<std::size_t>(bridge.engine()->sample_rate() / 10) * bridge.engine()->channels());
    bridge.engine()->read(mix);
    bridge.sync();
    CHECK(bridge.engine()->voice_count() == 0);
}

TEST_CASE("PhysicsBridge steps bodies, invokes fixed callback, emits contacts, and shifts origin") {
    Logger log;
    Profiler profiler;
    World world;
    FieldWorld fields;
    Time time{.fixed_delta = 1.0f / 60.0f};
    PhysicsBridge bridge(world, fields, time, &profiler, log, {.max_steps = 4, .interpolation = false});

    const Entity ground = world.create(Transform{.position = {0, 0, 0}},
                                       RigidBody{.desc = {.shape = ShapeDesc::box({10, 0.5f, 10}), .motion = MotionType::Static}});
    const Entity ball = world.create(Transform{.position = {0, 0.95f, 0}},
                                     RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f)}});

    int fixed_calls = 0;
    auto on_fixed = [](void* u) { ++(*static_cast<int*>(u)); };

    // 0 fixed steps when delta < fixed_delta.
    bridge.step(0.005f, on_fixed, &fixed_calls);
    CHECK(fixed_calls == 0);
    CHECK(bridge.world()->body_count() == 2);

    // Next delta triggers 2 fixed steps and produces a ground contact.
    EventReader<Contact> contact_reader;
    bridge.step(0.030f, on_fixed, &fixed_calls);
    CHECK(fixed_calls == 2);
    CHECK_FALSE(bridge.contacts().empty());
    CHECK(contact_reader.read(world, [](const Contact&) {}) > 0);

    // ScriptPhysics raycast resolves back to the ground entity.
    REQUIRE(bridge.script_physics() != nullptr);
    const auto hit = bridge.script_physics()->raycast({5, 5, 0}, {0, -1, 0}, 20.0f, 0xFFFFFFFFu, Entity{});
    REQUIRE(hit.has_value());
    CHECK(hit->entity == ground);

    // Origin shift moves simulated physics body positions.
    const float y_before = bridge.world()->position(world.get<RigidBody>(ball)->handle).y;
    bridge.shift_origin({0, 100.0f, 0});
    CHECK(bridge.world()->position(world.get<RigidBody>(ball)->handle).y == doctest::Approx(y_before - 100.0f));
}

TEST_CASE("RenderBridge syncs PrimitiveMesh and ModelRef headlessly and reports dimensions") {
    const auto dir = test::temp_dir("service_render_bridge");
    test::write_gltf(dir, "quad");

    Logger log;
    JobSystem jobs(1);
    World world;
    TypeRegistry types;
    register_world_types(types);
    types.add<ModelRef>();
    RenderBridge::register_types(types);
    AssetManager assets({.root = dir, .jobs = &jobs, .logger = &log, .hot_reload = false});
    RenderBridge bridge(world, assets, log, nullptr, nullptr, nullptr, nullptr,
                        {.width = 640, .height = 480, .offscreen = false, .asset_root = dir, .watch_materials = false});

    CHECK(bridge.screen_size().x == doctest::Approx(640.0f));
    CHECK(bridge.screen_size().y == doctest::Approx(480.0f));
    std::vector<std::uint8_t> rgba;
    CHECK_FALSE(bridge.read_frame(rgba));
    CHECK_FALSE(bridge.create_mesh(make_cube()).valid());
    CHECK_FALSE(bridge.create_material({}).valid());
    CHECK_FALSE(bridge.load_material("none.material").valid());

    // PrimitiveMesh -> MeshRenderer and removal.
    const Entity prim = world.create(Transform{}, PrimitiveMesh{});
    bridge.sync_primitives();
    CHECK(world.has<MeshRenderer>(prim));
    world.remove<PrimitiveMesh>(prim);
    bridge.sync_primitives();
    CHECK_FALSE(world.has<MeshRenderer>(prim));

    // ModelRef -> spawned pieces excluded from save_world.
    const Entity owner = world.create(Transform{}, ModelRef{"quad.gltf"});
    (void)assets.load_model("quad.gltf");
    assets.wait_all();
    bridge.sync_models();
    CHECK(world.count<Parent>() == 2);
    CHECK(save_world(world, types)["entities"].size() == 2); // prim + owner (no ModelPiece children)

    world.remove<ModelRef>(owner);
    bridge.sync_models();
    CHECK(world.count<Parent>() == 0);
}

TEST_CASE("ScriptBridge routes input, runs Luau scripts, loads actions, and writes API docs") {
    const auto dir = test::temp_dir("service_script_bridge");
    write_text(dir / "tick.luau", R"(
        astral.system { phase = "Update", run = function(ctx)
            if astral.input.down("fire") then astral.send("astral::TriggerEntered", {}) end
        end }
    )");
    write_text(dir / "controls.actions", R"({"actions": {"fire": {"type": "button", "keys": ["Space"]}}})");

    Logger log;
    JobSystem jobs(1);
    World world;
    TypeRegistry types;
    register_world_types(types);
    EventRegistry events;
    events.add<TriggerEntered>();
    InputActions actions;
    Time time{.fixed_delta = 1.0f / 60.0f, .delta = 1.0f / 60.0f};
    Profiler profiler;
    AssetManager assets({.root = dir, .jobs = &jobs, .logger = &log, .hot_reload = false});

    ScriptBridge bridge(world, assets, types, events, actions, time, &profiler, log, nullptr, dir / "saves", false);
    REQUIRE(bridge.host() != nullptr);
    REQUIRE(bridge.load_actions("controls.actions"));
    REQUIRE(bridge.load_script("tick.luau").empty());

    Input raw;
    raw.set_key(Key::Space, true);
    Input game_input;

    // Blocked keyboard prevents the action from firing.
    bridge.route_input(raw, game_input, true, false);
    CHECK_FALSE(actions.down("fire"));

    // Unblocked keyboard lets the action fire and the script observe it.
    EventReader<TriggerEntered> reader;
    bridge.route_input(raw, game_input, false, false);
    CHECK(actions.down("fire"));
    bridge.run(ScriptPhase::Update);
    CHECK(reader.read(world, [](const TriggerEntered&) {}) == 1);

    const auto api_dir = dir / "api";
    CHECK(bridge.write_script_api(api_dir.string(), {}));
    CHECK(std::filesystem::exists(api_dir / "astral.d.luau"));
    CHECK(std::filesystem::exists(api_dir / "API.md"));
}

TEST_CASE("WorldBridge resolves streaming sources, shifts origin, streams maps, and round-trips scenes") {
    const auto dir = test::temp_dir("service_world_bridge");
    write_text(dir / "maps" / "mini.world", R"({"format":"astral.map","version":1,
        "grids":[{"name":"main","cell_size":100,"load_range":150}]})");
    write_text(dir / "maps" / "mini.entities" / "1.ent",
               R"({"format":"astral.entity","version":1,"entity":{"id":1,"components":{"astral::Name":{"value":"rock"},
                   "astral::Transform":{"position":[20,0,20]}}}})");

    Logger log;
    JobSystem jobs(1);
    World world;
    TypeRegistry types;
    register_world_types(types);
    types.add<StreamingSource>();
    AssetManager assets({.root = dir, .jobs = &jobs, .logger = &log, .hot_reload = false});
    PrefabLibrary prefabs(types, [&](std::string_view path) {
        const AssetId id = assets.load_document(path);
        if (const Document* d = assets.document(id)) return SourceDocument{.json = &d->json, .version = d->version};
        return SourceDocument{.error = std::string(assets.error(id))};
    });
    WorldBridge bridge(world, prefabs, assets, jobs, log, dir, 500.0f, 1.0f);

    const Entity src = world.create(Transform{.position = {10, 0, 0}}, StreamingSource{});
    REQUIRE(bridge.open_world("maps/mini.world"));
    CHECK(bridge.is_open());

    auto sources = bridge.streaming_sources();
    REQUIRE(sources.size() == 1);
    CHECK(sources[0].x == doctest::Approx(10.0f));
    bridge.stream(sources, true);
    CHECK(find_by_name(world, "rock").valid());
    CHECK(bridge.debug_cells().has_value());

    // Origin shift past threshold shifts root Transform and emits OriginShifted.
    EventReader<OriginShifted> shifts;
    world.get<Transform>(src)->position.x = 650.0f;
    sources = bridge.streaming_sources();
    const auto offset = bridge.begin_origin_shift(sources);
    REQUIRE(offset.has_value());
    bridge.complete_origin_shift(*offset, sources);
    CHECK(shifts.read(world, [](const OriginShifted&) {}) == 1);
    CHECK(bridge.origin().x == doctest::Approx(700.0));

    bridge.close_world();
    CHECK_FALSE(bridge.is_open());

    // Scene save + load round-trips lighting.
    DirectionalLight sun{.intensity = 3.5f};
    Environment env{.exposure = 1.75f};
    CHECK(bridge.save_scene("saved.scene", sun, env));
    DirectionalLight loaded_sun{};
    Environment loaded_env{};
    CHECK(bridge.load_scene("saved.scene", loaded_sun, loaded_env).errors.empty());
    CHECK(loaded_sun.intensity == doctest::Approx(3.5f));
    CHECK(loaded_env.exposure == doctest::Approx(1.75f));
}

TEST_CASE("DevTools loads default font, draws overlays, and assembles HUD layers headlessly") {
    World world;
    DevTools tools(world, nullptr, nullptr, false);

    CHECK(tools.font() != nullptr);
    tools.begin_frame();
    { ASTRAL_PROFILE_SCOPE(&tools.profiler(), "unit"); }
    tools.end_frame();
    CHECK(tools.profiler().last_frame().size() == 1);

    world.create(Transform{}, RigidBody{.desc = {.shape = ShapeDesc::box({1, 1, 1})}});
    world.create(Transform{}, WindZone{.velocity = {2, 0, 0}});
    world.create(Transform{}, PointLight{});
    update_transforms(world);

    DebugDraw debug;
    Context::DebugShow show{.physics = true, .lights = true, .fields = true, .world_cells = false};
    tools.draw_overlays(show, debug, nullptr);
    CHECK_FALSE(debug.lines().empty());

    UiCanvas hud;
    CHECK(tools.overlay_layers(hud).empty());
    hud.rect({0, 0}, {10, 10}, {1, 1, 1, 1});
    CHECK(tools.overlay_layers(hud).size() == 1);
}

TEST_CASE("PH03: queued teleports move with the floating origin") {
    PhysicsCommandQueue queue;
    const Entity e{1, 1};
    queue.enqueue({.entity = e, .kind = PhysicsCommandKind::SetBodyTransform, .value = {2005, 1, -3000}});
    queue.enqueue({.entity = e, .kind = PhysicsCommandKind::Impulse, .value = {5, 0, 0}});
    queue.shift_positions({2000, 0, -3000});
    std::vector<PhysicsCommand> out;
    queue.drain_sorted(out);
    REQUIRE(out.size() == 2);
    for (const PhysicsCommand& c : out) {
        if (c.kind == PhysicsCommandKind::SetBodyTransform) {
            CHECK(c.value.x == doctest::Approx(5.0f));
            CHECK(c.value.z == doctest::Approx(0.0f));
        } else {
            CHECK(c.value.x == doctest::Approx(5.0f)); // a vector, not a position: unchanged
        }
    }
}
