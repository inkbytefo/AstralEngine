// Scripts inside the App (M11.5): phases, ctx.load_script, hot reload, actions.
#include "../../assets/tests/gltf_writer.hpp"

#include <astral/astral.hpp>
#include <doctest/doctest.h>

using namespace astral;

namespace {

void write_text(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    test::write_bytes(file, std::vector<std::uint8_t>(text.begin(), text.end()));
}

struct Counter {
    std::int32_t updates = 0;
    std::int32_t fixed = 0;
    std::int32_t post = 0;
};

} // namespace
ASTRAL_REFLECT(Counter, updates, fixed, post)

TEST_CASE("scripts run in the phases of the frame; the App's C++ systems and scripts share the world") {
    const auto dir = test::temp_dir("app_scripts");
    write_text(dir / "rules.luau", R"(
        astral.system { phase = "Update", query = {"Counter"}, update = function(ctx, e, c) c.updates = c.updates + 1 end }
        astral.system { phase = "FixedUpdate", query = {"Counter"}, update = function(ctx, e, c) c.fixed = c.fixed + 1 end }
        astral.system { phase = "PostUpdate", query = {"Counter"}, update = function(ctx, e, c) c.post = c.post + 1 end }
    )");
    write_text(dir / "game.actions", R"({"actions": {"fire": {"type": "button", "keys": ["Space"]}}})");

    struct G final : Game {
        Entity e{};
        std::vector<std::string> errors;
        bool actions_loaded = false;
        void on_start(Context& ctx) override {
            ctx.types.add<Counter>();
            errors = ctx.script_service().load_script("rules.luau");
            actions_loaded = ctx.script_service().load_actions("game.actions");
            e = ctx.world.create(Counter{});
        }
    } game;
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = dir.string(), .hot_reload = false});
    app.run_frames(game, 10);
    CHECK(game.errors.empty());
    CHECK(game.actions_loaded);
    CHECK(app.context().actions.has("fire"));
    const Counter* c = app.world().get<Counter>(game.e);
    REQUIRE(c);
    CHECK(c->updates == 10);
    CHECK(c->post == 10);
    CHECK(c->fixed >= 9); // one fixed step per frame at 60 Hz
}

TEST_CASE("a missing or broken script is reported with file and line, and the game keeps running") {
    const auto dir = test::temp_dir("app_scripts_bad");
    write_text(dir / "broken.luau", "local x = 1\nx = = 2\n");
    struct G final : Game {
        std::vector<std::string> missing, broken;
        void on_start(Context& ctx) override {
            missing = ctx.script_service().load_script("nothing.luau");
            broken = ctx.script_service().load_script("broken.luau");
        }
    } game;
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = dir.string(), .hot_reload = false});
    app.run_frames(game, 3);
    REQUIRE(game.missing.size() == 1);
    REQUIRE(game.broken.size() == 1);
    CHECK(game.broken[0].find("broken.luau:2") != std::string::npos);
}

TEST_CASE("editing a script file while the game runs reloads it; a bad edit keeps the last good version") {
    const auto dir = test::temp_dir("app_scripts_reload");
    const auto script = dir / "rules.luau";
    write_text(script, "astral.system { query = {\"Counter\"}, update = function(ctx, e, c) c.updates = c.updates + 1 end }");

    struct G final : Game {
        Entity e{};
        std::filesystem::path script;
        int frame = 0;
        std::int32_t after_first_edit = 0, after_bad_edit = 0;
        void on_start(Context& ctx) override {
            ctx.types.add<Counter>();
            ctx.script_service().load_script("rules.luau");
            e = ctx.world.create(Counter{});
        }
        void on_update(Context& ctx) override {
            ++frame;
            const Counter* c = ctx.world.get<const Counter>(e);
            if (frame == 5) write_text(script, "astral.system { query = {\"Counter\"}, update = function(ctx, e, c) c.updates = c.updates + 100 end }");
            if (frame == 70) {
                after_first_edit = c->updates;
                write_text(script, "this is not lua");
            }
            if (frame == 140) after_bad_edit = c->updates;
        }
    } game;
    game.script = script;
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = dir.string(), .hot_reload = true});
    app.run_frames(game, 141);
    CHECK(game.after_first_edit >= 100);                       // the edited version ran
    CHECK(app.world().get<Counter>(game.e)->updates > game.after_bad_edit - 1); // and kept running after the bad edit
    CHECK(game.after_bad_edit > game.after_first_edit);
}
