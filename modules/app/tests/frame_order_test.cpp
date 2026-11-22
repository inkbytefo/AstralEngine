// Characterization tests for App frame stage order and teardown order (M13.5 R2).
#include "../../assets/tests/gltf_writer.hpp"

#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

using namespace astral;

namespace {

struct TraceEvent {
    std::string tag;
};

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

class TraceSystem final : public System {
public:
    explicit TraceSystem(std::string tag) : tag_(std::move(tag)) {}
    void update(Context& ctx) override { ctx.world.send(TraceEvent{tag_}); }

private:
    std::string tag_;
};

std::vector<std::pair<std::string, std::uint16_t>> scope_trace(const Profiler& profiler) {
    std::vector<std::pair<std::string, std::uint16_t>> out;
    for (const ProfileSample& s : profiler.last_frame()) {
        out.emplace_back(s.name, s.depth);
    }
    return out;
}

constexpr std::string_view kPhaseScript = R"(
astral.system { phase = "PreUpdate",   run = function(ctx) astral.send("TraceEvent", { tag = "script:PreUpdate" }) end }
astral.system { phase = "Update",      run = function(ctx) astral.send("TraceEvent", { tag = "script:Update" }) end }
astral.system { phase = "FixedUpdate", run = function(ctx) astral.send("TraceEvent", { tag = "script:FixedUpdate" }) end }
astral.system { phase = "PostUpdate",  run = function(ctx) astral.send("TraceEvent", { tag = "script:PostUpdate" }) end }
)";

} // namespace

ASTRAL_REFLECT(TraceEvent, tag)

TEST_CASE("frame stage order with 0 fixed steps: C++ phases, scripts, and profiler scopes") {
    const auto dir = test::temp_dir("app_frame_order_0fixed");
    write_text(dir / "trace.luau", kPhaseScript);

    struct G final : Game {
        EventReader<TraceEvent> reader;
        std::vector<std::string> tags;
        void on_start(Context& ctx) override {
            ctx.events.add<TraceEvent>();
            REQUIRE(ctx.script_service().load_script("trace.luau").empty());
            ctx.add_system<TraceSystem>(Phase::PreUpdate, "cpp:PreUpdate");
            ctx.add_system<TraceSystem>(Phase::Update, "cpp:Update");
            ctx.add_system<TraceSystem>(Phase::FixedUpdate, "cpp:FixedUpdate");
            ctx.add_system<TraceSystem>(Phase::PostUpdate, "cpp:PostUpdate");
        }
        void on_update(Context& ctx) override { ctx.world.send(TraceEvent{"game:on_update"}); }
        void on_shutdown(Context& ctx) override {
            reader.read(ctx.world, [&](const TraceEvent& ev) { tags.push_back(ev.tag); });
        }
    } game;

    // physics_rate = 60 Hz (step = 1/60 s = 0.01667 s), fixed_timestep = 0.005 s => 0 fixed steps on frame 1.
    App app({.headless = true, .fixed_timestep = 0.005f, .physics_rate = 60.0f, .asset_root = dir.string(), .hot_reload = false});
    app.run_frames(game, 1);

    const std::vector<std::string> expected_tags{
        "cpp:PreUpdate", "script:PreUpdate",
        "game:on_update",
        "cpp:Update", "script:Update",
        "cpp:PostUpdate", "script:PostUpdate",
    };
    CHECK(game.tags == expected_tags);

    const std::vector<std::pair<std::string, std::uint16_t>> expected_scopes{
        {"world streaming", 0},
        {"assets", 0},
        {"PreUpdate", 0},
        {"TraceSystem", 1},
        {"scripts", 1},
        {"trace.luau", 2},
        {"Game::on_update", 0},
        {"Update", 0},
        {"TraceSystem", 1},
        {"scripts", 1},
        {"trace.luau", 2},
        {"physics", 0},
        {"PostUpdate", 0},
        {"TraceSystem", 1},
        {"scripts", 1},
        {"trace.luau", 2},
        {"transforms", 0},
        {"audio", 0},
        {"render", 0},
    };
    CHECK(scope_trace(*app.context().profiler) == expected_scopes);
}

TEST_CASE("frame stage order with 2 fixed steps: interleaved FixedUpdate, physics.step, and contact events") {
    const auto dir = test::temp_dir("app_frame_order_2fixed");
    write_text(dir / "trace.luau", kPhaseScript);

    struct Probe {
        Entity ball{};
        std::vector<float> physics_y_in_fixed;
        std::vector<float> transform_y_in_fixed;
        std::size_t contacts_in_update = 99;
        std::size_t contacts_in_post = 0;
        std::size_t contact_events_in_post = 0;
        float transform_y_in_post = 0.0f;
        EventReader<Contact> contact_reader;
        std::string pre_name;
        std::string update_name;
        std::string fixed_name;
        std::string post_name;
    } probe;

    class FixedProbe final : public System {
    public:
        explicit FixedProbe(Probe& p) : probe_(p) {}
        void update(Context& ctx) override {
            ctx.world.send(TraceEvent{"cpp:FixedUpdate"});
            const auto* rb = ctx.world.get<const RigidBody>(probe_.ball);
            const auto* t = ctx.world.get<const Transform>(probe_.ball);
            REQUIRE(rb != nullptr);
            REQUIRE(t != nullptr);
            probe_.physics_y_in_fixed.push_back(ctx.physics->position(rb->handle).y);
            probe_.transform_y_in_fixed.push_back(t->position.y);
        }

    private:
        Probe& probe_;
    };

    class UpdateProbe final : public System {
    public:
        explicit UpdateProbe(Probe& p) : probe_(p) {}
        void update(Context& ctx) override {
            ctx.world.send(TraceEvent{"cpp:Update"});
            probe_.contacts_in_update = ctx.physics_service().contacts().size();
        }

    private:
        Probe& probe_;
    };

    class PostProbe final : public System {
    public:
        explicit PostProbe(Probe& p) : probe_(p) {}
        void update(Context& ctx) override {
            ctx.world.send(TraceEvent{"cpp:PostUpdate"});
            probe_.contacts_in_post = ctx.physics_service().contacts().size();
            probe_.contact_events_in_post = probe_.contact_reader.read(ctx.world, [](const Contact&) {});
            probe_.transform_y_in_post = ctx.world.get<const Transform>(probe_.ball)->position.y;
        }

    private:
        Probe& probe_;
    };

    struct G final : Game {
        Probe* probe = nullptr;
        EventReader<TraceEvent> reader;
        std::vector<std::string> tags;
        void on_start(Context& ctx) override {
            ctx.events.add<TraceEvent>();
            REQUIRE(ctx.script_service().load_script("trace.luau").empty());
            // Static floor at y = 0 (top at y = 0.5) and a dynamic sphere touching it plus a falling sphere.
            ctx.world.create(Transform{.position = {0, 0, 0}},
                             RigidBody{.desc = {.shape = ShapeDesc::box({10, 0.5f, 10}), .motion = MotionType::Static}});
            ctx.world.create(Transform{.position = {3, 0.95f, 0}},
                             RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f)}});
            probe->ball = ctx.world.create(Transform{.position = {0, 10.0f, 0}},
                                           RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f)}});

            probe->pre_name = ctx.add_system<TraceSystem>(Phase::PreUpdate, "cpp:PreUpdate").name();
            probe->update_name = ctx.add_system<UpdateProbe>(Phase::Update, *probe).name();
            probe->fixed_name = ctx.add_system<FixedProbe>(Phase::FixedUpdate, *probe).name();
            probe->post_name = ctx.add_system<PostProbe>(Phase::PostUpdate, *probe).name();
        }
        void on_update(Context& ctx) override { ctx.world.send(TraceEvent{"game:on_update"}); }
        void on_shutdown(Context& ctx) override {
            reader.read(ctx.world, [&](const TraceEvent& ev) { tags.push_back(ev.tag); });
        }
    } game;
    game.probe = &probe;

    // 0.035 s at 60 Hz (0.01667 s) => exactly 2 fixed steps on frame 1.
    App app({.headless = true, .fixed_timestep = 0.035f, .physics_rate = 60.0f, .asset_root = dir.string(), .hot_reload = false});
    app.run_frames(game, 1);

    const std::vector<std::string> expected_tags{
        "cpp:PreUpdate", "script:PreUpdate",
        "game:on_update",
        "cpp:Update", "script:Update",
        "cpp:FixedUpdate", "script:FixedUpdate",
        "cpp:FixedUpdate", "script:FixedUpdate",
        "cpp:PostUpdate", "script:PostUpdate",
    };
    CHECK(game.tags == expected_tags);

    const std::vector<std::pair<std::string, std::uint16_t>> expected_scopes{
        {"world streaming", 0},
        {"assets", 0},
        {"PreUpdate", 0},
        {probe.pre_name, 1},
        {"scripts", 1},
        {"trace.luau", 2},
        {"Game::on_update", 0},
        {"Update", 0},
        {probe.update_name, 1},
        {"scripts", 1},
        {"trace.luau", 2},
        {"physics", 0},
        {"FixedUpdate", 1},
        {probe.fixed_name, 2},
        {"scripts", 2},
        {"trace.luau", 3},
        {"physics.step", 1},
        {"FixedUpdate", 1},
        {probe.fixed_name, 2},
        {"scripts", 2},
        {"trace.luau", 3},
        {"physics.step", 1},
        {"PostUpdate", 0},
        {probe.post_name, 1},
        {"scripts", 1},
        {"trace.luau", 2},
        {"transforms", 0},
        {"audio", 0},
        {"render", 0},
    };
    CHECK(scope_trace(*app.context().profiler) == expected_scopes);

    // Physics stepped between FixedUpdate #1 and #2, and Transform updated after the fixed loop before PostUpdate.
    REQUIRE(probe.physics_y_in_fixed.size() == 2);
    CHECK(probe.physics_y_in_fixed[0] == doctest::Approx(10.0f));
    CHECK(probe.physics_y_in_fixed[1] < 10.0f);
    CHECK(probe.transform_y_in_fixed[1] == doctest::Approx(10.0f)); // Transform not written until after fixed loop
    CHECK(probe.transform_y_in_post < probe.physics_y_in_fixed[1]);

    // Contacts are empty before physics, populated and emitted as events before PostUpdate.
    CHECK(probe.contacts_in_update == 0);
    CHECK(probe.contacts_in_post > 0);
    CHECK(probe.contact_events_in_post > 0);
}

TEST_CASE("world streaming, model sync, primitive sync, audio sync, and overlays occur at their documented frame points") {
    const auto dir = test::temp_dir("app_frame_order_services");
    test::write_gltf(dir, "quad");
    std::filesystem::create_directories(dir / "sounds");
    test::write_bytes(dir / "sounds" / "beep.wav", tone_wav(0.5f));
    write_text(dir / "maps" / "test.world", R"({"format":"astral.map","version":1,
        "grids":[{"name":"main","cell_size":100,"load_range":150}],
        "layers":[{"name":"night","active":false}]})");
    write_text(dir / "maps" / "test.entities" / "1.ent",
               R"({"format":"astral.entity","version":1,"entity":{"id":1,"components":{"astral::Name":{"value":"lamp"},
                   "astral::Transform":{"position":[10,0,10]}}},"layers":["night"]})");

    struct Observations {
        // Observed in frame 1 PostUpdate right after spawning components:
        bool f1_post_has_world_transform = true;
        bool f1_post_has_primitive_renderer = true;
        std::size_t f1_post_model_pieces = 99;
        std::uint32_t f1_post_voices = 99;
        bool f1_post_lamp_exists = true;

        // Observed in frame 2 PreUpdate (after frame 1 transforms/primitives/audio/render + frame 2 streaming/assets):
        bool f2_pre_has_world_transform = false;
        bool f2_pre_has_primitive_renderer = false;
        std::size_t f2_pre_model_pieces = 0;
        std::uint32_t f2_pre_voices = 0;
        bool f2_pre_lamp_exists = false;
        std::size_t f2_pre_debug_lines = 99;
    } obs;

    struct Entities {
        Entity prim{};
        Entity model{};
        Entity sound{};
    } ents;

    class PreObserver final : public System {
    public:
        PreObserver(Observations& obs, Entities& ents) : obs_(obs), ents_(ents) {}
        void update(Context& ctx) override {
            if (ctx.time.frame != 1) return; // second frame (0-indexed)
            obs_.f2_pre_has_world_transform = ctx.world.has<WorldTransform>(ents_.prim);
            obs_.f2_pre_has_primitive_renderer = ctx.world.has<MeshRenderer>(ents_.prim);
            obs_.f2_pre_model_pieces = ctx.world.count<Parent>();
            obs_.f2_pre_voices = ctx.audio->voice_count();
            obs_.f2_pre_lamp_exists = find_by_name(ctx.world, "lamp").valid();
            obs_.f2_pre_debug_lines = ctx.debug.lines().size();
        }

    private:
        Observations& obs_;
        Entities& ents_;
    };

    class PostSpawner final : public System {
    public:
        PostSpawner(Observations& obs, Entities& ents) : obs_(obs), ents_(ents) {}
        void update(Context& ctx) override {
            if (ctx.time.frame != 0) return; // first frame
            ctx.maps().world_layers()->set("night", true);
            ents_.prim = ctx.world.create(Transform{.position = {7, 0, 0}}, PrimitiveMesh{});
            ents_.model = ctx.world.create(Transform{}, ModelRef{"quad.gltf"});
            ents_.sound = ctx.world.create(Transform{}, AudioSource{.sound = "sounds/beep.wav", .loop = true});
            ctx.debug_show.lights = true;
            ctx.world.create(Transform{}, PointLight{});
            ctx.debug.line({0, 0, 0}, {1, 0, 0}, {1, 1, 1, 1}, {.seconds = 1.0f});

            obs_.f1_post_has_world_transform = ctx.world.has<WorldTransform>(ents_.prim);
            obs_.f1_post_has_primitive_renderer = ctx.world.has<MeshRenderer>(ents_.prim);
            obs_.f1_post_model_pieces = ctx.world.count<Parent>();
            obs_.f1_post_voices = ctx.audio->voice_count();
            obs_.f1_post_lamp_exists = find_by_name(ctx.world, "lamp").valid();
        }

    private:
        Observations& obs_;
        Entities& ents_;
    };

    struct G final : Game {
        Observations* obs = nullptr;
        Entities* ents = nullptr;
        void on_start(Context& ctx) override {
            REQUIRE(ctx.maps().open_world("maps/test.world"));
            ctx.world.create(Transform{}, StreamingSource{});
            ctx.maps().finish_world_loading();
            // Preload quad.gltf so sync_models can spawn pieces deterministically on the very next assets stage.
            (void)ctx.assets->load_model("quad.gltf");
            ctx.assets->wait_all();
            ctx.add_system<PreObserver>(Phase::PreUpdate, *obs, *ents);
            ctx.add_system<PostSpawner>(Phase::PostUpdate, *obs, *ents);
        }
    } game;
    game.obs = &obs;
    game.ents = &ents;

    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = dir.string(), .hot_reload = false});
    app.run_frames(game, 2);

    CHECK_FALSE(obs.f1_post_has_world_transform);
    CHECK_FALSE(obs.f1_post_has_primitive_renderer);
    CHECK(obs.f1_post_model_pieces == 0);
    CHECK(obs.f1_post_voices == 0);
    CHECK_FALSE(obs.f1_post_lamp_exists);

    CHECK(obs.f2_pre_has_world_transform);
    CHECK(obs.f2_pre_has_primitive_renderer);
    CHECK(obs.f2_pre_model_pieces == 2); // quad.gltf root + child node
    CHECK(obs.f2_pre_voices == 1);
    CHECK(obs.f2_pre_lamp_exists);
    CHECK(obs.f2_pre_debug_lines == 1); // timed line kept; one-frame light overlay lines cleared by debug.advance
}

TEST_CASE("teardown destroys systems in phase order after on_shutdown while engine services are still alive") {
    std::vector<std::string> teardown_log;
    bool all_services_alive = true;

    class TeardownSystem final : public System {
    public:
        TeardownSystem(std::string label, Context& ctx, std::vector<std::string>& log, bool& alive)
            : label_(std::move(label)), ctx_(ctx), log_(log), alive_(alive) {}
        void update(Context&) override {}
        ~TeardownSystem() override {
            log_.push_back(label_);
            const bool services_ok = ctx_.scripts != nullptr && ctx_.physics != nullptr && ctx_.audio != nullptr &&
                                     ctx_.assets != nullptr && ctx_.prefabs != nullptr && ctx_.font != nullptr &&
                                     ctx_.profiler != nullptr;
            if (services_ok) {
                (void)ctx_.physics->body_count();
                (void)ctx_.audio->voice_count();
            }
            alive_ = alive_ && services_ok;
        }

    private:
        std::string label_;
        Context& ctx_;
        std::vector<std::string>& log_;
        bool& alive_;
    };

    struct G final : Game {
        std::vector<std::string>* log = nullptr;
        bool* alive = nullptr;
        void on_start(Context& ctx) override {
            ctx.add_system<TeardownSystem>(Phase::PreUpdate, "~PreUpdate", ctx, *log, *alive);
            ctx.add_system<TeardownSystem>(Phase::Update, "~Update", ctx, *log, *alive);
            ctx.add_system<TeardownSystem>(Phase::FixedUpdate, "~FixedUpdate", ctx, *log, *alive);
            ctx.add_system<TeardownSystem>(Phase::PostUpdate, "~PostUpdate", ctx, *log, *alive);
        }
        void on_shutdown(Context&) override { log->push_back("on_shutdown"); }
    } game;
    game.log = &teardown_log;
    game.alive = &all_services_alive;

    {
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f});
        app.run_frames(game, 1);
    }

    const std::vector<std::string> expected{
        "on_shutdown",
        "~PreUpdate",
        "~Update",
        "~FixedUpdate",
        "~PostUpdate",
    };
    CHECK(teardown_log == expected);
    CHECK(all_services_alive);
}
