// Compatibility tests verifying grouped Context accessors match deprecated flat forwards (M13.5 R2).
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

// Isolated compatibility fixture: suppresses deprecation warnings solely when invoking the deprecated flat forwards.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

struct DeprecatedContextFixture {
    static LoadResult load_scene(Context& ctx, std::string_view p) { return ctx.load_scene(p); }
    static bool save_scene(Context& ctx, std::string_view p) { return ctx.save_scene(p); }
    static Entity spawn_prefab(Context& ctx, std::string_view p, const Transform& t = {}) { return ctx.spawn_prefab(p, t); }
    static bool open_world(Context& ctx, std::string_view p) { return ctx.open_world(p); }
    static void close_world(Context& ctx) { ctx.close_world(); }
    static void finish_world_loading(Context& ctx) { ctx.finish_world_loading(); }
    static MapLayers* world_layers(Context& ctx) noexcept { return ctx.world_layers(); }
    static const StreamingStats* world_streaming(const Context& ctx) noexcept { return ctx.world_streaming(); }
    static WorldPosition world_origin(const Context& ctx) noexcept { return ctx.world_origin(); }
    static MeshHandle create_mesh(Context& ctx, const MeshData& d) { return ctx.create_mesh(d); }
    static MaterialHandle create_material(Context& ctx, const MaterialDesc& d) { return ctx.create_material(d); }
    static bool update_material(Context& ctx, MaterialHandle m, const MaterialDesc& d) { return ctx.update_material(m, d); }
    static MaterialHandle load_material(Context& ctx, std::string_view p) { return ctx.load_material(p); }
    static std::string_view shader_error(const Context& ctx) noexcept { return ctx.shader_error(); }
    static SoundHandle sound(Context& ctx, std::string_view p) { return ctx.sound(p); }
    static std::vector<std::string> load_script(Context& ctx, std::string_view p) { return ctx.load_script(p); }
    static bool write_script_api(Context& ctx, std::string_view folder) { return ctx.write_script_api(folder); }
    static bool load_actions(Context& ctx, std::string_view p) { return ctx.load_actions(p); }
    static std::span<const ContactEvent> contacts(const Context& ctx) noexcept { return ctx.contacts(); }
};

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

} // namespace

TEST_CASE("Context preserves existing audio, scripts, and physics pointer field types") {
    static_assert(std::is_same_v<decltype(Context::audio), AudioEngine*>);
    static_assert(std::is_same_v<decltype(Context::scripts), ScriptHost*>);
    static_assert(std::is_same_v<decltype(Context::physics), PhysicsWorld*>);
    CHECK(true);
}

TEST_CASE("grouped Context accessors match deprecated flat forwards across all services") {
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
    write_text(dir / "sounds" / "tone.wav", "");
    test::write_bytes(dir / "sounds" / "tone.wav", tone_wav(0.1f));
    write_text(dir / "rules.luau", "astral.system { phase = 'Update', run = function(ctx) end }\n");
    write_text(dir / "input.actions", R"({"actions":{"jump":{"type":"button","keys":["Space"]}}})");

    struct Probe final : Game {
        const std::filesystem::path& dir;
        bool ok = false;
        explicit Probe(const std::filesystem::path& d) : dir(d) {}

        void on_start(Context& ctx) override {
            const Context& cctx = ctx;

            // Scenes: grouped vs deprecated
            const LoadResult r1 = ctx.scenes().load_scene("level.scene");
            const LoadResult r2 = DeprecatedContextFixture::load_scene(ctx, "level.scene");
            CHECK(r1.errors.empty());
            CHECK(r2.errors.empty());
            CHECK(r1.entities.size() == r2.entities.size());
            const Entity p1 = ctx.scenes().spawn_prefab("props/box.prefab", {.position = {1, 0, 0}});
            const Entity p2 = DeprecatedContextFixture::spawn_prefab(ctx, "props/box.prefab", {.position = {2, 0, 0}});
            CHECK(p1.valid());
            CHECK(p2.valid());
            CHECK(ctx.scenes().save_scene("out/grouped.scene"));
            CHECK(DeprecatedContextFixture::save_scene(ctx, "out/deprecated.scene"));
            CHECK(read_text(dir / "out" / "grouped.scene") == read_text(dir / "out" / "deprecated.scene"));

            // Maps: grouped vs deprecated
            REQUIRE(ctx.maps().open_world("maps/mini.world"));
            ctx.world.create(Transform{}, StreamingSource{});
            ctx.maps().finish_world_loading();
            CHECK(ctx.maps().world_layers() == DeprecatedContextFixture::world_layers(ctx));
            CHECK(cctx.maps().world_streaming() == DeprecatedContextFixture::world_streaming(cctx));
            CHECK(cctx.maps().world_origin().x == DeprecatedContextFixture::world_origin(cctx).x);
            CHECK(cctx.maps().world_origin().z == DeprecatedContextFixture::world_origin(cctx).z);
            DeprecatedContextFixture::close_world(ctx);
            CHECK(ctx.maps().world_layers() == nullptr);
            CHECK(cctx.maps().world_streaming() == nullptr);

            REQUIRE(DeprecatedContextFixture::open_world(ctx, "maps/mini.world"));
            DeprecatedContextFixture::finish_world_loading(ctx);
            CHECK(ctx.maps().world_layers() != nullptr);
            CHECK(find_by_name(ctx.world, "tree").valid());
            ctx.maps().close_world();
            CHECK(ctx.maps().world_layers() == nullptr);

            // Materials: grouped vs deprecated (headless safe invalid handles + identical error view)
            CHECK(ctx.materials().create_mesh(make_cube()) == DeprecatedContextFixture::create_mesh(ctx, make_cube()));
            CHECK(ctx.materials().create_material({}) == DeprecatedContextFixture::create_material(ctx, {}));
            CHECK(ctx.materials().update_material({}, {}) == DeprecatedContextFixture::update_material(ctx, {}, {}));
            CHECK(ctx.materials().load_material("missing.material") == DeprecatedContextFixture::load_material(ctx, "missing.material"));
            CHECK(cctx.materials().shader_error() == DeprecatedContextFixture::shader_error(cctx));

            // Audio: grouped vs deprecated return identical cached handles
            const SoundHandle s1 = ctx.audio_service().sound("sounds/tone.wav");
            const SoundHandle s2 = DeprecatedContextFixture::sound(ctx, "sounds/tone.wav");
            CHECK(s1.valid());
            CHECK(s1 == s2);

            // Scripts: grouped vs deprecated
            CHECK(ctx.script_service().load_script("rules.luau").empty());
            CHECK(DeprecatedContextFixture::load_script(ctx, "rules.luau").empty());
            CHECK(ctx.script_service().load_actions("input.actions"));
            CHECK(DeprecatedContextFixture::load_actions(ctx, "input.actions"));
            const auto api_grouped = dir / "api_grouped";
            const auto api_deprecated = dir / "api_deprecated";
            CHECK(ctx.script_service().write_script_api(api_grouped.string()));
            CHECK(DeprecatedContextFixture::write_script_api(ctx, api_deprecated.string()));
            CHECK(read_text(api_grouped / "API.md") == read_text(api_deprecated / "API.md"));
            CHECK(read_text(api_grouped / "astral.d.luau") == read_text(api_deprecated / "astral.d.luau"));

            // Physics contacts setup (body lands on floor to produce contact events)
            ctx.world.create(Transform{.position = {0, 0, 0}},
                             RigidBody{.desc = {.shape = ShapeDesc::box({10, 0.5f, 10}), .motion = MotionType::Static}});
            ctx.world.create(Transform{.position = {0, 0.95f, 0}},
                             RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f)}});
            ok = true;
        }
    };

    class ContactCompare final : public System {
    public:
        bool matched = false;
        void update(Context& ctx) override {
            const Context& cctx = ctx;
            const auto a = cctx.physics_service().contacts();
            const auto b = DeprecatedContextFixture::contacts(cctx);
            CHECK(a.data() == b.data());
            CHECK(a.size() == b.size());
            if (!a.empty()) matched = true;
        }
    };

    App app({.headless = true, .fixed_timestep = 1.0f / 30.0f, .asset_root = dir.string(), .hot_reload = false});
    Probe game(dir);
    auto& compare = app.context().add_system<ContactCompare>(Phase::PostUpdate);
    app.run_frames(game, 4);
    CHECK(game.ok);
    CHECK(compare.matched);
}
