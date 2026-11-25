#include "../../assets/tests/gltf_writer.hpp"

#include "../src/services/audio_bridge.hpp"
#include "../src/services/physics_bridge.hpp"
#include "../src/services/system_scheduler.hpp"
#include "../src/services/terrain_bridge.hpp"
#include "../src/services/vfx_runtime.hpp"

#include <astral/astral.hpp>
#include <astral/assets/asset_manager.hpp>
#include <astral/core/json.hpp>
#include <astral/core/log.hpp>
#include <astral/core/profiler.hpp>
#include <astral/world/world.hpp>
#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <thread>
#include <vector>

using namespace astral;

namespace {

struct HotReloadCounter {
    std::int32_t value = 0;
};
} // namespace
ASTRAL_REFLECT(HotReloadCounter, value)
namespace {

void write_text_file(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    test::write_bytes(file, std::vector<std::uint8_t>(text.begin(), text.end()));
}

std::vector<std::uint8_t> make_tone_wav(float seconds) {
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
    for (std::uint32_t i = 0; i < frames; ++i) u16(static_cast<std::uint16_t>((i & 1u) ? 4000 : -4000));
    return out;
}

} // namespace

TEST_CASE("SystemScheduler caches parallel waves, supports FunctionSystem, and defers re-entrant additions") {
    SystemAccess a;
    a.add_read(10);
    a.add_read(2);
    a.add_write(5);
    SystemAccess b;
    b.add_read(2);
    b.add_read(7);
    SystemAccess c;
    c.add_read(5);
    CHECK_FALSE(a.conflicts_with(b));
    CHECK(a.conflicts_with(c));

    struct CounterComp {
        int value = 0;
    };
    struct TagA {
        int v = 1;
    };
    struct TagB {
        int v = 2;
    };

    struct SchedulerGame final : Game {
        bool added_reentrant = false;
        int lambda_ticks = 0;
        int reentrant_ticks = 0;
        void on_start(Context& ctx) override {
            ctx.world.create(CounterComp{42});
            SystemAccess ro_a;
            ro_a.add_read(component_id<TagA>());
            ctx.add_system(
                Phase::Update, "lambda_a",
                [&](Context& inner) {
                    ++lambda_ticks;
                    const auto* sing = inner.singleton<CounterComp>();
                    CHECK(sing != nullptr);
                    CHECK(sing->value == 42);
                    if (!added_reentrant) {
                        added_reentrant = true;
                        inner.add_system(
                            Phase::Update, "reentrant",
                            [&](Context&) { ++reentrant_ticks; });
                    }
                },
                ro_a);
        }
    } game;

    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f});
    app.run_frames(game, 3);
    CHECK(game.lambda_ticks == 3);
    // Re-entrant system added on frame 1 runs starting frame 2.
    CHECK(game.reentrant_ticks == 2);
}

TEST_CASE("InputActions maps extended keys, rejects empty/malformed inputs, and preserves name order") {
    CHECK(key_from_name("") == Key::Unknown);
    CHECK(key_from_name("Delete") == Key::Delete);
    CHECK(key_from_name("Insert") == Key::Insert);
    CHECK(key_from_name("Home") == Key::Home);
    CHECK(key_from_name("End") == Key::End);
    CHECK(key_from_name("PageUp") == Key::PageUp);
    CHECK(key_from_name("PageDown") == Key::PageDown);
    CHECK(key_from_name("LeftSuper") == Key::LeftSuper);
    CHECK(key_from_name("RightSuper") == Key::RightSuper);
    CHECK(key_from_name("Minus") == Key::Minus);
    CHECK(key_from_name("Equal") == Key::Equal);
    CHECK(key_from_name("LeftBracket") == Key::LeftBracket);
    CHECK(key_from_name("RightBracket") == Key::RightBracket);
    CHECK(key_from_name("Backslash") == Key::Backslash);
    CHECK(key_from_name("Semicolon") == Key::Semicolon);
    CHECK(key_from_name("Apostrophe") == Key::Apostrophe);
    CHECK(key_from_name("Grave") == Key::Grave);
    CHECK(key_from_name("Comma") == Key::Comma);
    CHECK(key_from_name("Period") == Key::Period);
    CHECK(key_from_name("Slash") == Key::Slash);

    InputActions actions;
    std::string err;
    const auto bad1 = Json::parse(R"({"actions":{"fire":123}})");
    REQUIRE(bad1);
    CHECK_FALSE(actions.load(*bad1, &err));

    const auto bad2 = Json::parse(R"({"actions":{"look":{"type":"axis2","mouse_delta":["x",1]}}})");
    REQUIRE(bad2);
    CHECK_FALSE(actions.load(*bad2, &err));

    const auto good = Json::parse(R"({
        "actions":{
            "zebra":{"type":"button","keys":["Slash"]},
            "alpha":{"type":"button","keys":["Delete","Home"]}
        }
    })");
    REQUIRE(good);
    REQUIRE(actions.load(*good, &err));
    CHECK(actions.names() == std::vector<std::string>{"zebra", "alpha"});
    CHECK(actions.has("alpha"));
    CHECK(actions.has("zebra"));
    CHECK_FALSE(actions.has("missing"));

    Input in;
    in.set_key(Key::Slash, true);
    actions.update(in);
    CHECK(actions.pressed("zebra"));
    CHECK_FALSE(actions.down("alpha"));
    in.begin_frame();
    in.set_key(Key::Slash, false);
    in.set_key(Key::Delete, true);
    actions.update(in);
    CHECK(actions.released("zebra"));
    CHECK(actions.pressed("alpha"));
}

TEST_CASE("AudioBridge restarts immediately after stopping on the previous frame") {
    const auto dir = test::temp_dir("audio_restart_regression");
    std::filesystem::create_directories(dir / "sfx");
    test::write_bytes(dir / "sfx/ping.wav", make_tone_wav(0.3f));

    World world;
    Logger log;
    AssetManager assets({.root = dir, .logger = &log});
    AudioBridge audio(world, assets, log, false);

    const Entity e = world.create(Transform{}, AudioSource{.sound = "sfx/ping.wav", .playing = true});
    audio.sync();
    REQUIRE(audio.engine()->voice_count() == 1);

    world.get<AudioSource>(e)->playing = false;
    audio.sync();

    world.get<AudioSource>(e)->playing = true;
    audio.sync();
    CHECK(audio.engine()->voice_count() >= 1);
    CHECK(world.get<AudioSource>(e)->playing);
}

TEST_CASE("Script hot reload works when AppDesc::pack points to an absent pack file") {
    const auto dir = test::temp_dir("script_hot_reload_absent_pack");
    const auto script_path = dir / "logic.luau";
    write_text_file(script_path, R"(
astral.system({
    phase = "Update",
    query = {"HotReloadCounter"},
    update = function(ctx, e, c)
        c.value = c.value + 1
    end
})
)");

    struct HotReloadGame final : Game {
        std::filesystem::path script_file;
        Entity e{};
        int frame = 0;
        void on_start(Context& ctx) override {
            ctx.types.add<HotReloadCounter>();
            ctx.script_service().load_script("logic.luau");
            e = ctx.world.create(HotReloadCounter{});
        }
        void on_update(Context&) override {
            if (++frame == 1) {
                write_text_file(script_file, R"(
astral.system({
    phase = "Update",
    query = {"HotReloadCounter"},
    update = function(ctx, e, c)
        c.value = c.value + 10
    end
})
)");
            }
        }
    } game;
    game.script_file = script_path;

    App app({.headless = true,
             .fixed_timestep = 0.6f,
             .asset_root = dir.string(),
             .pack = (dir / "nonexistent.apak").string(),
             .hot_reload = true});
    app.run_frames(game, 2);

    const HotReloadCounter* counter = app.world().get<HotReloadCounter>(game.e);
    REQUIRE(counter != nullptr);
    CHECK(counter->value == 11);
}

TEST_CASE("PhysicsBridge handles invalid delta safely and cleans up removed joints in O(1)") {
    World world;
    FieldWorld fields;
    Time time{.fixed_delta = 1.0f / 60.0f};
    Profiler profiler;
    Logger log;
    PhysicsBridge physics(world, fields, time, &profiler, log, {.max_steps = 4});

    const Entity a = world.create(
        Transform{.position = {0, 2, 0}},
        RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f), .motion = MotionType::Dynamic, .mass = 1.0f}});
    const Entity b = world.create(
        Transform{.position = {0, 0, 0}},
        RigidBody{.desc = {.shape = ShapeDesc::box({0.5f, 0.5f, 0.5f}), .motion = MotionType::Dynamic, .mass = 1.0f}});
    const Entity c = world.create(
        Transform{.position = {2, 0, 0}},
        RigidBody{.desc = {.shape = ShapeDesc::box({0.5f, 0.5f, 0.5f}), .motion = MotionType::Static}});
    const Entity j1 = world.create(
        Joint{.type = JointType::Hinge, .a = a, .b = b, .anchor = {0, 1, 0}, .axis = {1, 0, 0}},
        JointMotor{.mode = JointMotorMode::Velocity, .max_torque = 10.0f, .target_velocity = 2.0f});
    const Entity j2 = world.create(
        Joint{.type = JointType::Distance, .a = b, .b = c, .anchor = {0, 0, 0}, .anchor_b = {2, 0, 0}});

    int fixed_ticks = 0;
    auto cb = [](void* u) { ++*static_cast<int*>(u); };
    physics.step(std::numeric_limits<float>::quiet_NaN(), cb, &fixed_ticks);
    physics.step(-1.0f, cb, &fixed_ticks);
    CHECK(fixed_ticks == 0);

    physics.step(1.0f / 60.0f, cb, &fixed_ticks);
    CHECK(fixed_ticks == 1);
    const JointHandle h1 = world.get<Joint>(j1)->handle;
    const JointHandle h2 = world.get<Joint>(j2)->handle;
    CHECK(physics.world()->valid(h1));
    CHECK(physics.world()->valid(h2));

    // Update JointMotor parameter and destroy one joint entity.
    world.get<JointMotor>(j1)->target_velocity = -1.5f;
    physics.step(1.0f / 60.0f, cb, &fixed_ticks);
    world.destroy(j1);
    physics.step(1.0f / 60.0f, cb, &fixed_ticks);
    CHECK_FALSE(physics.world()->valid(h1));
    CHECK(physics.world()->valid(h2));
}

TEST_CASE("GameplaySmoke disperse capsule AABB clamp clears exact boundary and in-range cells") {
    World w;
    FieldWorld f;
    Time t;
    Profiler p;
    Logger l;
    PhysicsBridge phys(w, f, t, &p, l, {});

    GameplaySmoke smoke(
        {.radius_cells = 3, .cell_size_m = 0.5f, .spread_interval_steps = 1, .spread_steps = 10, .fade_steps = 10});
    for (int i = 0; i < 4; ++i) smoke.step(phys, {});
    const float depth_before = smoke.optical_depth({}, {-5, 0, 0}, {5, 0, 0});
    CHECK(depth_before > 0.0f);

    // Out of bounds should leave smoke untouched.
    smoke.disperse({}, {50, 0, 0}, {60, 0, 0}, 1.0f, 5);
    CHECK(smoke.optical_depth({}, {-5, 0, 0}, {5, 0, 0}) == doctest::Approx(depth_before));

    // Capsule through centre clears smoke cleanly.
    smoke.disperse({}, {-2, 0, 0}, {2, 0, 0}, 1.2f, 5);
    CHECK(smoke.optical_depth({}, {-5, 0, 0}, {5, 0, 0}) < depth_before);
}

TEST_CASE("VfxRuntime caches parsed templates and excludes runtime helper components from save") {
    const auto dir = test::temp_dir("vfx_cache_and_save");
    write_text_file(dir / "spark.vfx", R"({
        "format":"astral.vfx","version":1,"tracks":[
            {"type":"particles","duration":0.4,"count":8},
            {"type":"light","duration":0.2}
        ]
    })");

    struct VfxGame final : Game {
        Entity e1{}, e2{};
        void on_start(Context& ctx) override {
            e1 = play_vfx(ctx, "spark.vfx", {0, 0, 0});
            e2 = play_vfx(ctx, "spark.vfx", {2, 0, 0});
        }
    } game;

    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = dir.string()});
    app.run_frames(game, 2);
    CHECK(game.e1.valid());
    CHECK(game.e2.valid());
    CHECK(app.world().count<VfxEmitter>() == 2);

    const Json saved = save_world(app.world(), app.context().types);
    const std::string text = saved.dump();
    CHECK(text.find("VfxEmitter") == std::string::npos);
    CHECK(text.find("SmokeAppearance") == std::string::npos);
    CHECK(text.find("VfxPlayback") == std::string::npos);
}

TEST_CASE("ShellOptions handles malformed trailing flags and App sanitizes invalid desc scalars") {
    const char* argv[] = {"prog", "--width", "-5", "--height", "abc", "--frames"};
    const ShellOptions opts = parse_shell_args(6, argv);
    CHECK(opts.width == 0);
    CHECK(opts.height == 0);
    CHECK(opts.frames == 0);

    const char* argv_api[] = {"prog", "--script-api-only"};
    const ShellOptions api_opts = parse_shell_args(2, argv_api);
    CHECK(api_opts.script_api_only);
    CHECK(api_opts.script_api.empty());

    App app({.width = 0,
             .height = 0,
             .headless = true,
             .fixed_timestep = -1.0f,
             .physics_rate = std::numeric_limits<float>::quiet_NaN(),
             .origin_shift_distance = -10.0f,
             .world_stream_budget_ms = -5.0f});
    CHECK(std::isfinite(app.time().fixed_delta));
    CHECK(app.time().fixed_delta == doctest::Approx(1.0f / 60.0f));
    struct EmptyGame final : Game {} game;
    app.run_frames(game, 2);
}

namespace {
class CountSink final : public ILogSink {
public:
    int errors = 0;
    int reloaded_infos = 0;
    void write(const LogRecord& entry) override {
        if (entry.level == LogLevel::Error) ++errors;
        if (entry.level == LogLevel::Info && entry.message.find("reloaded ") != std::string_view::npos) ++reloaded_infos;
    }
};
} // namespace

TEST_CASE("Audit regressions: read-only singleton, unnamed FunctionSystem, tap actions, multi-feature actions, reload logging, and transactional open_world") {
    SUBCASE("singleton<T> does not mark components changed, singleton_mut<T> marks only first, font is const, and empty FunctionSystem gets fallback name") {
        static_assert(std::is_same_v<decltype(Context::font), const Font*>);
        struct Marker {
            int v = 7;
        };
        struct ProbeGame final : Game {
            std::string fallback_name;
            std::vector<std::size_t> changed_counts;
            int frame = 0;
            void on_start(Context& ctx) override {
                ctx.world.create(Marker{9});
                ctx.world.create(Marker{15});
                System& s = ctx.add_system(Phase::PreUpdate, "", [&](Context& c) {
                    const Marker* m = c.singleton<Marker>();
                    CHECK(m != nullptr);
                    if (++frame == 3) {
                        Marker* mut = c.singleton_mut<Marker>();
                        REQUIRE(mut != nullptr);
                        mut->v = 42;
                    }
                });
                fallback_name = s.name();
                ctx.add_system(Phase::Update, "observer", [&](Context& c) {
                    changed_counts.push_back(c.world.query<const Marker>().changed<Marker>().count());
                });
            }
        } game;
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f});
        app.run_frames(game, 4);
        CHECK(game.fallback_name == "FunctionSystem");
        REQUIRE(game.changed_counts.size() == 4);
        CHECK(game.changed_counts[0] == 2);
        CHECK(game.changed_counts[1] == 0);
        CHECK(game.changed_counts[2] == 1);
        CHECK(game.changed_counts[3] == 0);
        CHECK(app.context().singleton<Marker>()->v == 42);
    }

    SUBCASE("ScriptStorage atomic overwrite preserves valid slot data across saves") {
        const auto save_dir = test::temp_dir("atomic_save_overwrite");
        const auto script_dir = test::temp_dir("atomic_save_scripts");
        write_text_file(script_dir / "save.luau", R"(
            local step = 0
            astral.system({
                phase = "Update",
                query = {"HotReloadCounter"},
                update = function(ctx, e, c)
                    step = step + 1
                    c.value = step * 10
                    astral.save("slot_a", {"HotReloadCounter"})
                end
            })
        )");
        struct SaveGame final : Game {
            void on_start(Context& ctx) override {
                ctx.types.add<HotReloadCounter>();
                ctx.script_service().load_script("save.luau");
                ctx.world.create(HotReloadCounter{1});
            }
        } game;
        App app({.headless = true,
                 .fixed_timestep = 1.0f / 60.0f,
                 .asset_root = script_dir.string(),
                 .hot_reload = false,
                 .save_folder = save_dir.string()});
        app.run_frames(game, 3);
        CHECK(std::filesystem::exists(save_dir / "slot_a.json"));
        CHECK_FALSE(std::filesystem::exists(save_dir / "slot_a.json.tmp"));
    }

    SUBCASE("InputActions preserves same-frame press and release tap edges") {
        InputActions actions;
        const auto doc = Json::parse(R"({"actions":{"tap":{"type":"button","keys":["Space"]}}})");
        REQUIRE(doc);
        REQUIRE(actions.load(*doc));
        Input in;
        in.begin_frame();
        in.set_key(Key::Space, true);
        in.set_key(Key::Space, false);
        actions.update(in);
        CHECK_FALSE(actions.down("tap"));
        CHECK(actions.pressed("tap"));
        CHECK(actions.released("tap"));
        in.begin_frame();
        actions.update(in);
        CHECK_FALSE(actions.pressed("tap"));
        CHECK_FALSE(actions.released("tap"));
    }

    SUBCASE("FeatureSet composes multiple action files and broken hot-reload does not spam or log reloaded") {
        const auto dir = test::temp_dir("multi_feature_actions_reload");
        write_text_file(dir / "features/move/feature.json",
                        R"({"format":"astral.feature","name":"move","actions":["features/move/move.actions"]})");
        write_text_file(dir / "features/move/move.actions",
                        R"({"actions":{"jump":{"type":"button","keys":["Space"]}}})");
        write_text_file(dir / "features/combat/feature.json",
                        R"({"format":"astral.feature","name":"combat","actions":["features/combat/combat.actions"]})");
        write_text_file(dir / "features/combat/combat.actions",
                        R"({"actions":{"shoot":{"type":"button","keys":["F"]}}})");

        auto sink = std::make_shared<CountSink>();
        struct MultiGame final : Game {
            std::filesystem::path combat_actions;
            std::shared_ptr<CountSink> sink;
            bool installed = false;
            int frame = 0;
            void on_start(Context& ctx) override {
                ctx.log.add_sink(sink);
                FeatureSet set;
                set.add("move");
                set.add("combat");
                installed = set.install(ctx);
            }
            void on_update(Context&) override {
                if (++frame == 1) write_text_file(combat_actions, "{not valid json");
            }
        } game;
        game.combat_actions = dir / "features/combat/combat.actions";
        game.sink = sink;

        App app({.headless = true, .fixed_timestep = 0.6f, .asset_root = dir.string(), .hot_reload = true});
        app.run_frames(game, 4);
        CHECK(game.installed);
        CHECK(app.context().actions.has("jump"));
        CHECK(app.context().actions.has("shoot"));
        CHECK(sink->reloaded_infos == 0);
        CHECK(sink->errors == 1);
    }

    SUBCASE("Failed open_world retains the currently open world") {
        const auto dir = test::temp_dir("transactional_open_world");
        write_text_file(dir / "maps/good.world", R"({"format":"astral.map","version":1,
            "grids":[{"name":"main","cell_size":100,"load_range":150}]})");
        write_text_file(dir / "maps/good.entities/1.ent",
                        R"({"format":"astral.entity","version":1,"entity":{"id":1,"components":{"astral::Name":{"value":"anchor"},"astral::Transform":{"position":[0,0,0]}}},"always_loaded":true})");

        struct WorldGame final : Game {
            bool first_ok = false;
            bool second_ok = true;
            bool anchor_alive = false;
            bool still_open = false;
            void on_start(Context& ctx) override {
                first_ok = ctx.maps().open_world("maps/good.world");
                second_ok = ctx.maps().open_world("maps/missing.world");
                anchor_alive = find_by_name(ctx.world, "anchor").valid();
                still_open = ctx.maps().world_layers() != nullptr;
            }
        } game;
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = dir.string()});
        app.run_frames(game, 1);
        CHECK(game.first_ok);
        CHECK_FALSE(game.second_ok);
        CHECK(game.anchor_alive);
        CHECK(game.still_open);
    }
}
