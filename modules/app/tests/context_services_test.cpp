// Tests for grouped Context services (M13.5 R2, updated in F1 ADR-0034).
#include "../../assets/tests/gltf_writer.hpp"

#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

using namespace astral;

namespace {

void write_text(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    test::write_bytes(file, std::vector<std::uint8_t>(text.begin(), text.end()));
}

std::string read_text(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
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

TEST_CASE("Context preserves existing audio, scripts, and physics pointer field types") {
    static_assert(std::is_same_v<decltype(Context::audio), AudioEngine*>);
    static_assert(std::is_same_v<decltype(Context::scripts), ScriptHost*>);
    static_assert(std::is_same_v<decltype(Context::physics), PhysicsWorld*>);
    CHECK(true);
}

TEST_CASE("grouped Context services cover scenes, maps, materials, audio, scripts, and physics") {
    const auto dir = test::temp_dir("context_grouped_services");
    write_text(dir / "props" / "box.prefab",
               R"({"format":"astral.scene","version":1,"entities":[{"id":1,"components":{"astral::Name":{"value":"box"},"astral::Transform":{}}}]})");
    write_text(dir / "level.scene",
               R"({"format":"astral.scene","version":1,"environment":{"exposure":1.25},"entities":[
                   {"id":1,"components":{"astral::Name":{"value":"floor"},"astral::Transform":{}}}]})");
    write_text(dir / "maps" / "mini.world",
               R"({"format":"astral.map","version":1,"grids":[{"name":"main","cell_size":100,"load_range":150}],
                   "layers":[{"name":"night","active":false}]})");
    write_text(dir / "maps" / "mini.entities" / "1.ent",
               R"({"format":"astral.entity","version":1,"entity":{"id":1,"components":{"astral::Name":{"value":"tree"},
                   "astral::Transform":{"position":[10,0,10]}}}})");
    std::filesystem::create_directories(dir / "sounds");
    test::write_bytes(dir / "sounds" / "tone.wav", tone_wav(0.1f));
    write_text(dir / "rules.luau", "astral.system { phase = 'Update', run = function(ctx) end }\n");
    write_text(dir / "input.actions", R"({"actions":{"jump":{"type":"button","keys":["Space"]}}})");

    struct Probe final : Game {
        const std::filesystem::path& dir;
        bool ok = false;
        explicit Probe(const std::filesystem::path& d) : dir(d) {}

        void on_start(Context& ctx) override {
            const Context& cctx = ctx;

            const LoadResult r = ctx.scenes().load_scene("level.scene");
            CHECK(r.errors.empty());
            CHECK(r.entities.size() == 1);
            CHECK(ctx.scenes().spawn_prefab("props/box.prefab", {.position = {1, 0, 0}}).valid());
            CHECK(ctx.scenes().save_scene("out/grouped.scene"));
            CHECK_FALSE(read_text(dir / "out" / "grouped.scene").empty());

            REQUIRE(ctx.maps().open_world("maps/mini.world"));
            CHECK(cctx.maps().resolve_map_entity(1).status == MapEntityStatus::Unloaded);
            ctx.world.create(Transform{}, StreamingSource{});
            ctx.maps().finish_world_loading();
            CHECK(ctx.maps().world_layers() != nullptr);
            CHECK(cctx.maps().world_streaming() != nullptr);
            CHECK(cctx.maps().world_origin().x == 0.0);
            const MapEntityLookup alive = cctx.maps().resolve_map_entity(1);
            CHECK(alive.status == MapEntityStatus::Alive);
            CHECK(alive.entity.valid());
            CHECK(alive.load_count == 1);
            ctx.maps().close_world();
            CHECK(ctx.maps().world_layers() == nullptr);
            CHECK(cctx.maps().world_streaming() == nullptr);

            CHECK_FALSE(ctx.materials().create_mesh(make_cube()).valid());
            CHECK_FALSE(ctx.materials().create_material({}).valid());
            CHECK_FALSE(ctx.materials().update_material({}, {}));
            CHECK_FALSE(ctx.materials().load_material("missing.material").valid());
            CHECK(cctx.materials().shader_error().empty());

            CHECK(ctx.audio_service().sound("sounds/tone.wav").valid());

            CHECK(ctx.script_service().load_script("rules.luau").empty());
            CHECK(ctx.script_service().load_actions("input.actions"));
            const auto api_dir = dir / "api_grouped";
            CHECK(ctx.script_service().write_script_api(api_dir.string()));
            CHECK_FALSE(read_text(api_dir / "API.md").empty());
            CHECK_FALSE(read_text(api_dir / "astral.d.luau").empty());

            ctx.world.create(Transform{.position = {0, 0, 0}},
                             RigidBody{.desc = {.shape = ShapeDesc::box({10, 0.5f, 10}), .motion = MotionType::Static}});
            ctx.world.create(Transform{.position = {0, 0.95f, 0}},
                             RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f)}});
            ok = true;
        }
    };

    class ContactObserver final : public System {
    public:
        bool saw_contacts = false;
        void update(Context& ctx) override {
            if (!ctx.physics_service().contacts().empty()) saw_contacts = true;
        }
    };

    App app({.headless = true, .fixed_timestep = 1.0f / 30.0f, .asset_root = dir.string(), .hot_reload = false});
    Probe game(dir);
    auto& observer = app.context().add_system<ContactObserver>(Phase::PostUpdate);
    app.run_frames(game, 4);
    CHECK(game.ok);
    CHECK(observer.saw_contacts);
}
