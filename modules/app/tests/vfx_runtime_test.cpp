#include "../../assets/tests/gltf_writer.hpp"
#include "../src/services/vfx_runtime.hpp"

#include <astral/astral.hpp>
#include <astral/assets/cook.hpp>
#include <astral/assets/pack.hpp>
#include <doctest/doctest.h>

#include <fstream>

using namespace astral;

TEST_CASE("VFX timelines play and clean up identically from source and pack") {
    const auto dir = test::temp_dir("vfx_timeline");
    std::filesystem::create_directories(dir / "source");
    std::ofstream(dir / "source/burst.vfx") << R"({
        "format":"astral.vfx","version":1,"tracks":[
            {"type":"particles","duration":0.3,"count":16},
            {"type":"volume","start":0.02,"duration":0.25,"material":"flame.material"},
            {"type":"light","start":0.05,"duration":0.12},
            {"type":"smoke","start":0.1,"duration":0.2},
            {"type":"decal","start":0.15,"duration":0.2}
        ]
    })";
    std::ofstream(dir / "source/bad.vfx") << R"({"format":"astral.vfx","version":2,"tracks":[]})";
    REQUIRE(cook_directory({.source = dir / "source", .output = dir / "cooked"}).ok());
    REQUIRE(write_pack(dir / "cooked", dir / "vfx.apak"));

    struct Demo final : Game {
        Entity effect{};
        unsigned frame = 0;
        std::vector<unsigned> states;
        void on_start(Context& ctx) override {
            effect = play_vfx(ctx, "burst.vfx", {5, 0, 0});
            REQUIRE(effect.valid());
            CHECK_FALSE(play_vfx(ctx, "bad.vfx", {}).valid());
        }
        void on_update(Context& ctx) override {
            const unsigned state = (ctx.world.count<VfxEmitter>() ? 1u : 0u) | (ctx.world.count<PointLight>() ? 2u : 0u) |
                (ctx.world.count<GameplaySmoke>() ? 4u : 0u) | (ctx.world.count<Decal>() ? 8u : 0u) |
                (ctx.world.count<VfxVolume>() ? 16u : 0u);
            states.push_back(state);
            if (++frame == 14) stop_vfx(ctx, effect);
        }
    };

    const std::string root = (dir / "source").string();
    const std::string path = (dir / "vfx.apak").string();
    const auto run = [&](bool packed) {
        Demo demo;
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .worker_threads = 2, .asset_root = root,
                 .pack = packed ? std::string_view(path) : std::string_view{}, .hot_reload = false});
        CHECK(app.context().assets->from_pack() == packed);
        app.run_frames(demo, 30);
        CHECK(app.world().count() == 0);
        return demo.states;
    };
    const auto loose = run(false);
    CHECK(loose == run(true));
    REQUIRE(loose.size() == 30);
    CHECK(loose[1] == 1);
    CHECK((loose[4] & 20u) == 16u);
    CHECK((loose[11] & 29u) == 29u);
    CHECK(loose[14] == 0);
}

TEST_CASE("Luau VFX events start and stop a tagged effect once") {
    const auto dir = test::temp_dir("vfx_luau");
    std::ofstream(dir / "burst.vfx") << R"({"format":"astral.vfx","version":1,
        "tracks":[{"type":"particles","duration":1}]})";
    std::ofstream(dir / "effect.luau") << R"(
        local step = 0
        astral.system {
            phase = "FixedUpdate",
            run = function()
                step += 1
                if step == 1 then
                    astral.send("astral::PlayVfx", {asset = "burst.vfx", position = vector.create(4, 2, 1), tag = "burst"})
                elseif step == 3 then
                    astral.send("astral::StopVfx", {tag = "burst"})
                end
            end
        }
    )";
    struct Demo final : Game {
        unsigned frame = 0;
        void on_start(Context& ctx) override {
            REQUIRE(ctx.script_service().load_script("effect.luau").empty());
        }
        void on_update(Context& ctx) override {
            ++frame;
            if (frame == 2 || frame == 3) CHECK(ctx.world.count<VfxEmitter>() == 1);
            if (frame >= 4) CHECK(ctx.world.count<VfxEmitter>() == 0);
        }
    } demo;
    const std::string root = dir.string();
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = root, .hot_reload = false});
    app.run_frames(demo, 6);
    CHECK(app.context().scripts->take_errors().empty());
}
