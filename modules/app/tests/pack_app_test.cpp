#include "../../assets/tests/gltf_writer.hpp"

#include <astral/assets/cook.hpp>
#include <astral/assets/pack.hpp>
#include <astral/astral.hpp>
#include <doctest/doctest.h>

using namespace astral;

namespace {
void write_text(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    test::write_bytes(file, std::vector<std::uint8_t>(text.begin(), text.end()));
}
} // namespace

TEST_CASE("a shipped game reads scenes, prefabs and models from its .apak; without one, loose files") {
    const auto src = test::temp_dir("app_pack_src");
    test::write_gltf(src / "props", "quad");
    write_text(src / "props" / "lamp.prefab", R"({"format": "astral.scene", "version": 1, "entities": [
        {"id": 1, "components": {"astral::Name": {"value": "lamp"}, "astral::Transform": {},
                                 "astral::ModelRef": {"path": "props/quad.gltf"}}},
        {"id": 2, "components": {"astral::Name": {"value": "bulb"}, "astral::Parent": {"entity": 1},
                                 "astral::PointLight": {"intensity": 10}}}]})");
    write_text(src / "level.scene", R"({"format": "astral.scene", "version": 1, "entities": [
        {"id": 1, "prefab": "props/lamp.prefab", "parts": {"2": {"components": {"astral::PointLight": {"intensity": 30}}}}}]})");
    const auto cooked = test::temp_dir("app_pack_cooked");
    REQUIRE(cook_directory({.source = src, .output = cooked, .textures = {}, .jobs = nullptr, .logger = nullptr}).ok());
    const auto pack = test::temp_dir("app_pack_file") / "game.apak";
    REQUIRE(write_pack(cooked, pack));

    struct G final : Game {
        bool from_pack = false;
        std::size_t model_pieces = 0;
        float intensity = 0;
        void on_start(Context& ctx) override {
            from_pack = ctx.assets->from_pack();
            const LoadResult r = ctx.load_scene("level.scene");
            REQUIRE(r.errors.empty());
            intensity = ctx.world.get<PointLight>(find_part(ctx.world, r.entities[0], "bulb"))->intensity;
        }
        void on_update(Context& ctx) override {
            ctx.assets->wait_all();
            model_pieces = ctx.world.count<MeshRenderer>();
        }
    };
    const std::string pack_path = pack.string();
    const std::string missing = (pack.parent_path() / "nope.apak").string();
    const std::string nowhere = (src / "not-here").string();
    for (const bool shipped : {true, false}) {
        CAPTURE(shipped);
        G game;
        // Shipped: the pack exists, the source folder does not matter. Development: no pack -> loose files.
        App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = shipped ? nowhere : src.string(),
                 .pack = shipped ? pack_path : missing, .hot_reload = false});
        app.run_frames(game, 3);
        CHECK(game.from_pack == shipped);
        CHECK(game.intensity == 30.0f);
        CHECK(game.model_pieces == 2); // quad model: two nodes with a mesh each
    }
}
