// G1.1 item 4: AppDesc::input_script feeds scripted key changes to headless games (tests, demos).
#include "../../assets/tests/gltf_writer.hpp"

#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace astral;

namespace {

void write_text(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    test::write_bytes(file, std::vector<std::uint8_t>(text.begin(), text.end()));
}

struct Restarts {
    int count = 0;
};

} // namespace

ASTRAL_REFLECT(Restarts, count)

TEST_CASE("G1.1 AppDesc::input_script drives named actions in C++ and Luau without a window") {
    const auto dir = test::temp_dir("g1_input_script");
    write_text(dir / "game.actions", R"({"format": "astral.actions", "version": 1, "actions": {
        "restart": {"type": "button", "keys": ["R"]}
    }})");
    write_text(dir / "restart.luau", R"(
        astral.system {
            phase = "Update",
            run = function()
                if astral.input.pressed("restart") then
                    local counter = astral.find("counter")
                    local r = counter:get("Restarts")
                    r.count += 1
                end
            end
        }
    )");

    struct G final : Game {
        std::vector<std::uint64_t> pressed_frames, down_frames;
        Entity counter{};
        void on_start(Context& ctx) override {
            ctx.types.add<Restarts>();
            counter = ctx.world.create(Name{"counter"}, Restarts{});
            REQUIRE(ctx.script_service().load_actions("game.actions"));
            REQUIRE(ctx.script_service().load_script("restart.luau").empty());
        }
        void on_update(Context& ctx) override {
            if (ctx.actions.pressed("restart")) pressed_frames.push_back(ctx.time.frame);
            if (ctx.input.down(Key::R)) down_frames.push_back(ctx.time.frame);
        }
    } game;

    const InputScriptEvent script[] = {
        {.frame = 6, .key = Key::R, .down = true}, // unsorted on purpose: the App orders by frame
        {.frame = 2, .key = Key::R, .down = true},
        {.frame = 4, .key = Key::R, .down = false},
    };
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = dir.string(), .hot_reload = false,
             .input_script = script});
    app.run_frames(game, 8);
    CHECK(app.context().scripts->take_errors().empty());
    CHECK(game.pressed_frames == std::vector<std::uint64_t>{2, 6});
    CHECK(game.down_frames == std::vector<std::uint64_t>{2, 3, 6, 7});
    CHECK(app.world().get<const Restarts>(game.counter)->count == 2);
}

TEST_CASE("G1.1 the App keeps its own copy of the input script") {
    std::vector<InputScriptEvent> script{{.frame = 1, .key = Key::Space, .down = true}};
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .input_script = script});
    script.clear();
    script.shrink_to_fit();
    struct G final : Game {
        bool seen = false;
        void on_update(Context& ctx) override { seen = seen || ctx.input.pressed(Key::Space); }
    } game;
    app.run_frames(game, 3);
    CHECK(game.seen);
}
