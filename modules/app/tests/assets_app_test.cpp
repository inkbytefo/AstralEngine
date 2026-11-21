#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../assets/tests/gltf_writer.hpp"

#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <thread>

using namespace astral;

TEST_CASE("a ModelRef entity gets the model's node tree as children; they are never saved") {
    const auto dir = test::temp_dir("app_modelref");
    test::write_gltf(dir, "quad");
    const std::string root = dir.string();
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = root, .hot_reload = false});
    struct G final : Game {
        Entity owner{};
        std::size_t pieces = 0, renderers = 0, saved = 0;
        int frame = 0;
        void on_start(Context& ctx) override {
            owner = ctx.world.create(Transform{.position = {5, 0, 0}}, ModelRef{"quad.gltf"});
        }
        void on_update(Context& ctx) override {
            if (frame == 0) ctx.assets->wait_all(); // loads are asynchronous: like a loading screen
            if (++frame == 4) {
                pieces = ctx.world.count<Parent>();
                renderers = ctx.world.count<MeshRenderer>();
                saved = save_world(ctx.world, ctx.types)["entities"].size();
            }
            if (frame == 5) ctx.world.remove<ModelRef>(owner); // pieces go away next frame
        }
    } game;
    app.run_frames(game, 4);
    CHECK(game.pieces == 2);    // base node + child node
    CHECK(game.renderers == 2);
    CHECK(game.saved == 1);     // just the owner with its ModelRef: the pieces are regenerated on load
}

TEST_CASE("pieces follow the owner: removing the ModelRef or destroying the owner removes them") {
    const auto dir = test::temp_dir("app_modelref_remove");
    test::write_gltf(dir, "quad");
    const std::string root = dir.string();
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = root, .hot_reload = false});
    struct G final : Game {
        Entity a{}, b{};
        std::vector<std::size_t> counts;
        int frame = 0;
        void on_start(Context& ctx) override {
            a = ctx.world.create(Transform{}, ModelRef{"quad.gltf"});
            b = ctx.world.create(Transform{}, ModelRef{"quad.gltf"});
        }
        void on_update(Context& ctx) override {
            if (frame == 0) ctx.assets->wait_all();
            counts.push_back(ctx.world.count<MeshRenderer>());
            ++frame;
            if (frame == 3) ctx.world.remove<ModelRef>(a);
            if (frame == 5) ctx.world.destroy(b);
        }
    } game;
    app.run_frames(game, 8);
    CHECK(game.counts[2] == 4); // two models x two pieces
    CHECK(game.counts[4] == 2); // a's pieces removed
    CHECK(game.counts[7] == 0); // b's pieces removed
}

