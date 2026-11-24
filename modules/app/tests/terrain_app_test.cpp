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

TEST_CASE("terrain LOD stitching preserves projected tile area for every edge combination") {
    TerrainManifest manifest{.samples = 9, .tiles = {}};
    const TerrainTileSource tile{.file = {}};
    std::vector<float> heights(81, 0.0f);
    for (unsigned mask = 0; mask < 16; ++mask) {
        CAPTURE(mask);
        const auto mesh = build_terrain_mesh(manifest, tile, heights, {.stitched_edges = mask});
        float area = 0.0f;
        for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
            const auto& a = mesh.vertices[mesh.indices[i]].position;
            const auto& b = mesh.vertices[mesh.indices[i + 1]].position;
            const auto& c = mesh.vertices[mesh.indices[i + 2]].position;
            const float twice_area = cross(b - a, c - a).y;
            CHECK(twice_area > 0);
            area += twice_area * 0.5f;
            for (const auto v : {a, b, c}) {
                if ((v.x == 0 && (mask & (1u << TerrainWest))) || (v.x == 8 && (mask & (1u << TerrainEast)))) {
                    CHECK(static_cast<unsigned>(v.z) % 2 == 0);
                }
                if ((v.z == 0 && (mask & (1u << TerrainNorth))) || (v.z == 8 && (mask & (1u << TerrainSouth)))) {
                    CHECK(static_cast<unsigned>(v.x) % 2 == 0);
                }
            }
        }
        CHECK(area == 64);
    }
}

TEST_CASE("fine and coarse terrain tiles share edge heights, UVs and normals") {
    TerrainManifest manifest{.samples = 5, .tiles = {}};
    std::vector<float> west(25), east(25);
    for (unsigned z = 0; z < 5; ++z) {
        for (unsigned x = 0; x < 5; ++x) {
            west[z * 5 + x] = static_cast<float>(x + 2 * z);
            east[z * 5 + x] = static_cast<float>(4 + 2 * x * x + 2 * z);
        }
    }
    TerrainMeshLod fine{.stitched_edges = 1u << TerrainEast};
    fine.neighbors[TerrainEast] = east;
    TerrainMeshLod coarse{.stride = 2};
    coarse.neighbors[TerrainWest] = west;
    const auto a = build_terrain_mesh(manifest, {.file = {}}, west, fine);
    const auto b = build_terrain_mesh(manifest, {.x = 1, .file = {}}, east, coarse);
    REQUIRE(b.vertices.size() == 9);
    CHECK(b.indices.size() == 24);
    for (unsigned row = 0; row < 3; ++row) {
        const auto av = a.vertices[row * 10 + 4];
        const auto bv = b.vertices[row * 3];
        CHECK(av.position == bv.position + Vec3{4, 0, 0});
        CHECK(av.normal == bv.normal);
        CHECK(av.uv == bv.uv);
    }
}

TEST_CASE("character crosses all four terrain tiles and the shared corner") {
    const auto dir = test::temp_dir("terrain_character");
    const std::vector<std::uint16_t> samples(65 * 65, 0);
    test::write_bytes(dir / "flat.aheight", write_terrain_tile(samples));
    std::ofstream(dir / "terrain.aterrain") << R"({
        "format":"astral.terrain","version":1,"samples":65,"spacing_m":1,"min_height_m":0,"max_height_m":1,
        "tiles":[{"x":0,"z":0,"file":"flat.aheight"},{"x":1,"z":0,"file":"flat.aheight"},
                 {"x":0,"z":1,"file":"flat.aheight"},{"x":1,"z":1,"file":"flat.aheight"}]
    })";
    std::ofstream(dir / "level.scene") << R"({"format":"astral.scene","version":1,"entities":[
        {"id":1,"components":{"astral::Transform":{},"astral::TerrainRef":{"asset":"terrain.aterrain"}}}
    ]})";
    struct CrossingGame final : Game {
        Entity character{};
        unsigned frame = 0;
        unsigned visited = 0;
        bool crossed_corner = false;
        void on_start(Context& ctx) override {
            REQUIRE(ctx.scenes().load_scene("level.scene").errors.empty());
            character = ctx.world.create(Transform{.position = {61, 0.05f, 61}}, CharacterController{});
        }
        void on_update(Context& ctx) override {
            auto* c = ctx.world.get<CharacterController>(character);
            const auto p = ctx.world.get<const Transform>(character)->position;
            ++frame;
            if (frame < 20) return;
            CHECK(c->on_ground);
            CHECK(std::abs(p.y) < 0.08f);
            crossed_corner |= std::abs(p.x - 64) < 0.16f && std::abs(p.z - 64) < 0.16f;
            visited |= 1u << ((p.x >= 64 ? 1u : 0u) + (p.z >= 64 ? 2u : 0u));
            if (frame <= 79) c->move = {6, 0, 0};
            else if (frame <= 139) c->move = {0, 0, 6};
            else if (frame <= 199) c->move = {-6, 0, 0};
            else c->move = {6, 0, -6};
        }
    } game;
    const std::string root = dir.string();
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = root, .hot_reload = false});
    app.run_frames(game, 260);
    CHECK(game.visited == 15);
    CHECK(game.crossed_corner);

    struct CameraGame final : Game {
        Entity camera{};
        unsigned frame = 0;
        BodyHandle body{};
        float height = 0.0f;
        MeshHandle near_mesh{};
        void on_start(Context& ctx) override {
            REQUIRE(ctx.scenes().load_scene("level.scene").errors.empty());
            camera = ctx.world.create(Transform{.position = {32, 20, 32}}, Camera{});
        }
        void on_update(Context& ctx) override {
            if (++frame == 1) return;
            const auto hit = ctx.physics->raycast({32, 10, 32}, {0, -1, 0}, 20);
            REQUIRE(hit);
            MeshHandle mesh;
            ctx.world.each<const TerrainTileDraw, const Transform, const MeshRenderer>(
                [&](const TerrainTileDraw&, const Transform& t, const MeshRenderer& r) {
                    if (t.position == Vec3{}) mesh = r.mesh;
                });
            REQUIRE(mesh.valid());
            if (frame == 2) {
                body = hit->body;
                height = hit->position.y;
                near_mesh = mesh;
                ctx.world.get<Transform>(camera)->position.x = 250;
            } else {
                CHECK(mesh != near_mesh);
                CHECK(hit->body == body);
                CHECK(hit->position.y == height);
            }
        }
    } camera_game;
    App render_app({.width = 64, .height = 64, .fixed_timestep = 1.0f / 60.0f,
                    .render = {.ray_tracing = false}, .asset_root = root, .hot_reload = false, .offscreen = true});
    if (!render_app.context().renderer) return;
    render_app.run_frames(camera_game, 3);
}
