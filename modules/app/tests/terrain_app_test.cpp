#include "../../assets/tests/gltf_writer.hpp"
#include "../src/services/terrain_bridge.hpp"

#include <astral/astral.hpp>
#include <doctest/doctest.h>

#include <cmath>
#include <fstream>

using namespace astral;

TEST_CASE("a translated 65-sample scene terrain matches the rendered triangle heights and releases its body") {
    const auto dir = test::temp_dir("terrain_app");
    constexpr std::uint32_t n = 65;
    std::vector<std::uint16_t> samples(n * n, 0);
    samples[0] = 65535;
    samples[1] = 32768;
    samples[n + 1] = 49152;
    test::write_bytes(dir / "tile.aheight", write_terrain_tile(samples));
    std::ofstream(dir / "terrain.aterrain") << R"({
        "format":"astral.terrain","version":1,"samples":65,"spacing_m":1,"min_height_m":0,"max_height_m":8,
        "tiles":[{"x":-1,"z":0,"file":"tile.aheight"}]
    })";
    std::ofstream(dir / "level.scene") << R"({"format":"astral.scene","version":1,"entities":[
        {"id":1,"components":{"astral::Transform":{"position":[100,2,20]},
        "astral::TerrainRef":{"asset":"terrain.aterrain"}}}
    ]})";

    struct TerrainGame final : Game {
        Entity owner{};
        BodyHandle body{};
        unsigned frame = 0;
        void on_start(Context& ctx) override {
            const auto loaded = ctx.scenes().load_scene("level.scene");
            REQUIRE(loaded.errors.empty());
            REQUIRE(loaded.entities.size() == 1);
            owner = loaded.entities.front();
        }
        void on_update(Context& ctx) override {
            if (++frame != 2) return;
            const auto hit = [&](float x, float z) {
                return ctx.physics->raycast({x, 20, z}, {0, -1, 0}, 30);
            };
            const auto& ref = *ctx.world.get<const TerrainRef>(owner);
            CHECK(ref.asset == "terrain.aterrain");
            REQUIRE(ctx.scenes().save_scene("saved.scene"));
            TerrainManifest manifest{.samples = n, .min_height_m = 0, .max_height_m = 8, .tiles = {}};
            std::vector<float> heights(n * n, 0);
            heights[0] = 8;
            heights[1] = manifest.height_m(32768);
            heights[n + 1] = manifest.height_m(49152);
            const auto mesh = build_terrain_mesh(manifest, {.x = -1, .z = 0, .file = {}}, heights);
            REQUIRE(mesh.indices.size() == 64 * 64 * 6);
            for (unsigned triangle = 0; triangle < 2; ++triangle) {
                Vec3 centre{};
                for (unsigned i = 0; i < 3; ++i) centre = centre + mesh.vertices[mesh.indices[triangle * 3 + i]].position;
                centre = centre / 3.0f;
                const auto ray = hit(36 + centre.x, 20 + centre.z);
                REQUIRE(ray);
                body = ray->body;
                CHECK(entity_of(*ray) == owner);
                CHECK(std::abs(ray->position.y - (2 + centre.y)) <= 0.025f);
            }
            const auto far_corner = hit(99.99f, 83.99f);
            REQUIRE(far_corner);
            CHECK(far_corner->position.y == doctest::Approx(2.0f).epsilon(0.005));
            CHECK_FALSE(hit(100.01f, 83.99f));
            ctx.world.destroy(owner);
        }
    } game;
    const std::string root = dir.string();
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = root, .hot_reload = false});
    app.run_frames(game, 3);
    REQUIRE(game.body.valid());
    CHECK_FALSE(app.context().physics->valid(game.body));
    std::ifstream file(dir / "saved.scene");
    const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const auto doc = Json::parse(text);
    REQUIRE(doc);
    CHECK((*doc)["entities"].size() == 1);
    CHECK((*doc)["entities"][0]["components"]["astral::TerrainRef"]["asset"].as_string() == "terrain.aterrain");
}
