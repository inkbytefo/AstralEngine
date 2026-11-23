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

TEST_CASE("a saved model slot override is scoped to its owner and can be cleared") {
    const auto dir = test::temp_dir("app_model_material_override");
    test::write_glb(dir, "quad");
    const std::string asset = R"({"format":"astral.material","version":1,"type":"pbr","values":{"base_color":[0,0,1,1]}})";
    test::write_bytes(dir / "blue.material", std::vector<std::uint8_t>(asset.begin(), asset.end()));
    const std::string root = dir.string();
    App app({.width = 80, .height = 60, .render = {.ray_tracing = false}, .asset_root = root,
             .hot_reload = false, .offscreen = true});
    if (!app.context().renderer) return;

    struct G final : Game {
        Entity a, b;
        MaterialHandle blue, original;
        void on_start(Context& ctx) override {
            const ModelRef source{"quad.glb", {{"painted", "blue.material"}}};
            const Json saved = to_json(source, {});
            ModelRef loaded;
            REQUIRE(from_json(saved, loaded, {}));
            REQUIRE(loaded.material_overrides == source.material_overrides);
            a = ctx.world.create(Transform{}, loaded);
            b = ctx.world.create(Transform{}, ModelRef{"quad.glb"});
            blue = ctx.materials().load_material("blue.material");
            REQUIRE(blue.valid());
        }
        MaterialHandle root_material(Context& ctx, Entity owner) {
            MaterialHandle handle{};
            ctx.world.each<const Parent, const MeshRenderer>([&](const Parent& parent, const MeshRenderer& mesh) {
                if (parent.entity == owner) handle = mesh.material;
            });
            return handle;
        }
        void on_update(Context& ctx) override {
            if (ctx.time.frame == 0) ctx.assets->wait_all();
            if (ctx.time.frame == 3) {
                original = root_material(ctx, b);
                REQUIRE(original.valid());
                CHECK(original != blue);
                CHECK(root_material(ctx, a) == blue);
                ctx.world.get<ModelRef>(a)->material_overrides.clear();
            }
            if (ctx.time.frame == 5) CHECK(root_material(ctx, a) == original);
        }
    } game;
    app.run_frames(game, 6);
}
