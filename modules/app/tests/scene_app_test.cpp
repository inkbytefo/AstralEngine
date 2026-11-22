#include "../../assets/tests/gltf_writer.hpp"

#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <fstream>
#include <sstream>

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

constexpr std::string_view lamp = R"({"format": "astral.scene", "version": 1, "entities": [
  {"id": 1, "components": {"astral::Name": {"value": "lamp"}, "astral::Transform": {},
                            "astral::PrimitiveMesh": {"shape": "Cube", "material": {"base_color": [0.2, 0.2, 0.2, 1]}}}},
  {"id": 2, "components": {"astral::Name": {"value": "bulb"}, "astral::Parent": {"entity": 1},
                            "astral::Transform": {"position": [0, 1, 0]},
                            "astral::PrimitiveMesh": {"shape": "Sphere", "material": {"emissive": [4, 4, 3]}},
                            "astral::PointLight": {"intensity": 10}}}]})";

constexpr std::string_view level = R"({"format": "astral.scene", "version": 1,
  "environment": {"exposure": 0.5},
  "entities": [
    {"id": 1, "components": {"astral::Name": {"value": "floor"},
                             "astral::Transform": {"scale": [10, 1, 10]},
                             "astral::PrimitiveMesh": {"shape": "Plane"}}},
    {"id": 2, "prefab": "props/lamp.prefab", "components": {"astral::Transform": {"position": [3, 0, 0]}},
     "parts": {"2": {"components": {"astral::PointLight": {"intensity": 25}}}}}]})";

} // namespace

TEST_CASE("the App loads scenes, spawns prefabs and saves scenes that load back the same") {
    const auto dir = test::temp_dir("app_scenes");
    write_text(dir / "props" / "lamp.prefab", lamp);
    write_text(dir / "level.scene", level);
    const std::string root = dir.string();

    struct G final : Game {
        LoadResult loaded;
        Entity spawned{}, by_code{};
        std::size_t spawned_parts = 0, by_code_parts = 0, renderers = 0;
        float bulb_intensity = 0, exposure = 0;
        int frame = 0;
        bool saved = false;
        void on_start(Context& ctx) override {
            loaded = ctx.scenes().load_scene("level.scene");
            exposure = ctx.environment.exposure;
            spawned = ctx.scenes().spawn_prefab("props/lamp.prefab", {.position = {-3, 0, 0}});
            spawned_parts = ctx.world.query<const PrefabPart>().count();
        }
        void on_update(Context& ctx) override {
            if (++frame == 1) by_code = ctx.world.create(Transform{.position = {0, 0, -3}}, PrefabInstance{"props/lamp.prefab"});
            if (frame == 3) {
                by_code_parts = ctx.world.query<const PrefabPart>().count();
                renderers = ctx.world.count<MeshRenderer>();
                const Entity lamp2 = loaded.entities[1];
                bulb_intensity = ctx.world.get<PointLight>(find_part(ctx.world, lamp2, "bulb"))->intensity;
                ctx.world.get<PointLight>(find_part(ctx.world, spawned, "bulb"))->intensity = 2; // an edit to save
                saved = ctx.scenes().save_scene("out/saved.scene");
            }
        }
    } game;
    {
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = root, .hot_reload = false});
        app.run_frames(game, 3);
    }
    CHECK(game.loaded.errors.empty());
    CHECK(game.loaded.entities.size() == 2);
    CHECK(game.exposure == 0.5f);
    CHECK(game.spawned_parts == 2);  // the scene's lamp and spawn_prefab's lamp: one bulb each, at once
    CHECK(game.by_code_parts == 3);  // plus the instance made with world.create, spawned by the App
    CHECK(game.bulb_intensity == 25.0f);
    CHECK(game.renderers == 7);      // floor + 3 lamps x (base + bulb), from PrimitiveMesh
    REQUIRE(game.saved);

    const std::string first = read_text(dir / "out" / "saved.scene");
    auto doc = Json::parse(first);
    REQUIRE(doc);
    const Json& list = (*doc)["entities"];
    REQUIRE(list.size() == 4); // floor, 3 lamp instances (their parts are overrides, not records)
    CHECK((*doc)["environment"]["exposure"].as_number() == 0.5);
    CHECK(list[1]["parts"]["2"]["components"]["astral::PointLight"]["intensity"].as_number() == 25);
    CHECK(list[2]["parts"]["2"]["components"]["astral::PointLight"]["intensity"].as_number() == 2);
    CHECK(list[2]["components"]["astral::Transform"]["position"][0].as_number() == -3);
    CHECK(list[3]["components"]["astral::Transform"]["position"][2].as_number() == -3);

    // Load the saved scene into a new App and save it again: the same file.
    struct H final : Game {
        bool saved = false;
        void on_start(Context& ctx) override {
            CHECK(ctx.scenes().load_scene("out/saved.scene").errors.empty());
            saved = ctx.scenes().save_scene("out/again.scene");
        }
    } again;
    {
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = root, .hot_reload = false});
        app.run_frames(again, 1);
    }
    REQUIRE(again.saved);
    CHECK(read_text(dir / "out" / "again.scene") == first);
}

TEST_CASE("editing or removing a PrimitiveMesh updates the drawn mesh") {
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .hot_reload = false});
    struct G final : Game {
        Entity e{};
        int frame = 0;
        bool had_renderer = false, removed = false;
        void on_start(Context& ctx) override { e = ctx.world.create(Transform{}, PrimitiveMesh{}); }
        void on_update(Context& ctx) override {
            ++frame;
            if (frame == 2) {
                had_renderer = ctx.world.has<MeshRenderer>(e);
                ctx.world.remove<PrimitiveMesh>(e);
            }
            if (frame == 3) removed = !ctx.world.has<MeshRenderer>(e);
        }
    } game;
    app.run_frames(game, 3);
    CHECK(game.had_renderer);
    CHECK(game.removed);
}
