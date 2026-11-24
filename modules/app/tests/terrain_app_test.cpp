#include "../../assets/tests/gltf_writer.hpp"
#include "../src/services/terrain_bridge.hpp"

#include <astral/astral.hpp>
#include <astral/assets/cook.hpp>
#include <astral/assets/pack.hpp>
#include <doctest/doctest.h>

#include <algorithm>
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

TEST_CASE("terrain streaming holds a character until collision arrives and preserves negative coordinates after origin shift") {
    const auto dir = test::temp_dir("terrain_streaming");
    const auto source = dir / "source";
    std::filesystem::create_directories(source);
    const std::vector<std::uint16_t> flat(65 * 65, 0);
    test::write_bytes(source / "flat.aheight", write_terrain_tile(flat));
    std::ofstream(source / "terrain.aterrain") << R"({
        "format":"astral.terrain","version":1,"samples":65,"spacing_m":1,"min_height_m":0,"max_height_m":1,
        "tiles":[{"x":-1,"z":0,"file":"flat.aheight"},{"x":0,"z":0,"file":"flat.aheight"},
                 {"x":4,"z":0,"file":"flat.aheight"}]
    })";
    REQUIRE(cook_directory({.source = source, .output = dir / "cooked"}).ok());
    REQUIRE(write_pack(dir / "cooked", dir / "terrain.apak"));

    struct StreamGame final : Game {
        Entity terrain{}, focus{}, character{};
        BodyHandle first{}, far_body{};
        unsigned frame = 0;
        float held_x = 0;
        float moved_x = 0;
        void on_start(Context& ctx) override {
            terrain = ctx.world.create(Transform{.position = {-3000, 0, 0}},
                TerrainRef{.asset = "terrain.aterrain", .material_file = {}, .render_radius_m = 2, .collision_radius_m = 4});
            focus = ctx.world.create(Transform{.position = {-3032, 0, 32}}, StreamingSource{});
            character = ctx.world.create(Transform{.position = {-3000.5f, 0.04f, 32}},
                                          CharacterController{.move = {120, 0, 0}});
        }
        void on_update(Context& ctx) override {
            ++frame;
            const float origin = ctx.world.get<const Transform>(terrain)->position.x;
            auto ray = [&](float offset) {
                return ctx.physics->raycast({origin + offset, 5, 32}, {0, -1, 0}, 10);
            };
            if (frame == 1) {
                CHECK(ctx.maps().world_origin().x == doctest::Approx(-3000.0));
                CHECK_FALSE(ray(-32));
            }
            if (frame == 2) {
                held_x = ctx.world.get<const Transform>(character)->position.x - origin;
                CHECK(held_x == doctest::Approx(-0.5f));
                ctx.maps().finish_world_loading();
                const auto near = ray(-32);
                const auto across = ray(32);
                REQUIRE(near);
                REQUIRE(across);
                first = near->body;
                CHECK(entity_of(*near) == terrain);
                CHECK_FALSE(ray(288));

                ctx.world.get<Transform>(focus)->position.x = origin + 60;
                ctx.maps().finish_world_loading();
                CHECK(ctx.maps().world_origin().x == doctest::Approx(-2900.0));
                const auto rebased = ctx.physics->raycast({-132, 5, 32}, {0, -1, 0}, 10);
                REQUIRE(rebased);
                CHECK(rebased->body == first);
                CHECK(rebased->position.y == near->position.y);
            }
            if (frame == 3) {
                moved_x = ctx.world.get<const Transform>(character)->position.x - origin;
                CHECK(moved_x > 0);
                ctx.world.destroy(character);
                ctx.world.get<Transform>(focus)->position.x = origin + 288;
                ctx.maps().finish_world_loading();
                CHECK_FALSE(ctx.physics->valid(first));
                const float rebased_origin = ctx.world.get<const Transform>(terrain)->position.x;
                const auto far = ctx.physics->raycast({rebased_origin + 288, 5, 32}, {0, -1, 0}, 10);
                REQUIRE(far);
                far_body = far->body;
            }
            if (frame == 4) {
                ctx.world.get<Transform>(focus)->position.x = origin - 32;
                ctx.maps().finish_world_loading();
                CHECK_FALSE(ctx.physics->valid(far_body));
                const float rebased_origin = ctx.world.get<const Transform>(terrain)->position.x;
                REQUIRE(ctx.physics->raycast({rebased_origin - 32, 5, 32}, {0, -1, 0}, 10));
                ctx.world.destroy(terrain);
            }
        }
    };
    const std::string source_path = source.string();
    const std::string packed_path = (dir / "terrain.apak").string();
    const auto run = [&](bool packaged) {
        StreamGame game;
        App app({.headless = true, .fixed_timestep = 1.0f / 60, .worker_threads = 2,
                 .asset_root = source_path, .pack = packaged ? std::string_view(packed_path) : std::string_view{},
                 .hot_reload = false, .origin_shift_distance = 50});
        CHECK(app.context().assets->from_pack() == packaged);
        app.run_frames(game, 4);
        CHECK_FALSE(app.context().physics->raycast({-32, 5, 32}, {0, -1, 0}, 10));
        return game.moved_x;
    };
    CHECK(run(false) == run(true));
}

TEST_CASE("terrain streaming bounds resident collision over a two kilometer terrain") {
    const auto dir = test::temp_dir("terrain_two_km");
    const std::vector<std::uint16_t> flat(65 * 65, 0);
    test::write_bytes(dir / "flat.aheight", write_terrain_tile(flat));
    Json manifest = Json::object();
    manifest["format"] = "astral.terrain";
    manifest["version"] = 1;
    manifest["samples"] = 65;
    manifest["spacing_m"] = 1;
    manifest["min_height_m"] = 0;
    manifest["max_height_m"] = 1;
    manifest["tiles"] = Json::array();
    for (int z = 0; z < 32; ++z) {
        for (int x = 0; x < 32; ++x) {
            Json tile = Json::object();
            tile["x"] = x;
            tile["z"] = z;
            tile["file"] = "flat.aheight";
            manifest["tiles"].push_back(std::move(tile));
        }
    }
    std::ofstream(dir / "terrain.aterrain") << manifest.dump();

    struct LargeGame final : Game {
        Entity focus{}, camera{};
        unsigned max_colliders = 0, max_meshes = 0;
        void on_start(Context& ctx) override {
            ctx.world.create(Transform{}, TerrainRef{.asset = "terrain.aterrain", .material_file = {},
                                                     .render_radius_m = 64, .collision_radius_m = 80});
            focus = ctx.world.create(Transform{}, StreamingSource{});
            camera = ctx.world.create(Transform{.rotation = Quat::from_axis_angle({1, 0, 0}, radians(-40.0f))}, Camera{});
        }
        void on_update(Context& ctx) override {
            const auto origin = ctx.maps().world_origin();
            auto& p = ctx.world.get<Transform>(focus)->position;
            const float x = static_cast<float>(step * 320 + 32);
            p = {x - static_cast<float>(origin.x), 0, 32};
            ctx.world.get<Transform>(camera)->position = p + Vec3{0, 60, 50};
            ctx.maps().finish_world_loading();
            unsigned resident = 0;
            for (int z = 0; z < 32; ++z) {
                for (int tx = 0; tx < 32; ++tx) {
                    const Vec3 point{static_cast<float>(tx * 64 + 32) - static_cast<float>(origin.x),
                                      10, static_cast<float>(z * 64 + 32)};
                    resident += ctx.physics->raycast(point, {0, -1, 0}, 20).has_value() ? 1u : 0u;
                }
            }
            CHECK(resident >= 4);
            CHECK(resident <= 12);
            max_colliders = std::max(max_colliders, resident);
            max_meshes = std::max(max_meshes, static_cast<unsigned>(ctx.world.count<TerrainTileDraw>()));
            ++step;
        }
        unsigned step = 0;
    };
    const auto root = dir.string();
    for (const bool headless : {true, false}) {
        LargeGame game;
        App app({.width = 160, .height = 90, .headless = headless, .fixed_timestep = 1.0f / 60, .worker_threads = 2,
                 .render = {.ray_tracing = false}, .asset_root = root, .hot_reload = false, .offscreen = !headless});
        if (!headless && !app.context().renderer) continue;
        app.run_frames(game, 6);
        CHECK(game.max_colliders <= 12);
        CHECK(game.max_meshes <= game.max_colliders);
        if (!headless) CHECK(game.max_meshes > 0);
        auto frame_times = app.context().profiler->frame_times();
        REQUIRE(frame_times.size() == 6);
        std::sort(frame_times.begin(), frame_times.end());
        MESSAGE("terrain 2048m run. headless=", headless, ", peak colliders=", game.max_colliders,
                ", peak meshes=", game.max_meshes, ", median frame ms=",
                (frame_times[2] + frame_times[3]) * 0.5f, ", max frame ms=", frame_times.back(),
                ", final LOD build ms=", app.context().profiler->scope_ms("terrain LOD mesh build"),
                ", final total mesh upload ms=", app.context().profiler->scope_ms("terrain mesh upload"));
    }
}

TEST_CASE("terrain streaming discards cancelled work and retries a missing tile after re-entry") {
    const auto dir = test::temp_dir("terrain_stream_recovery");
    const std::vector<std::uint16_t> heights(65 * 65, 0);
    test::write_bytes(dir / "valid.aheight", write_terrain_tile(heights));
    std::ofstream(dir / "terrain.aterrain") << R"({
        "format":"astral.terrain","version":1,"samples":65,"spacing_m":1,"min_height_m":0,"max_height_m":1,
        "tiles":[{"x":0,"z":0,"file":"valid.aheight"},{"x":4,"z":0,"file":"late.aheight"}]
    })";
    struct RecoverGame final : Game {
        Entity focus{}, owner{};
        unsigned frame = 0;
        std::filesystem::path dir;
        std::vector<std::uint8_t> payload;
        void on_start(Context& ctx) override {
            focus = ctx.world.create(Transform{.position = {32, 0, 32}}, StreamingSource{});
            owner = create_terrain(ctx);
        }
        Entity create_terrain(Context& ctx) {
            return ctx.world.create(Transform{}, TerrainRef{.asset = "terrain.aterrain", .material_file = {},
                                                             .render_radius_m = 4, .collision_radius_m = 8});
        }
        void on_update(Context& ctx) override {
            ++frame;
            const auto hit = [&](float x) { return ctx.physics->raycast({x, 5, 32}, {0, -1, 0}, 10); };
            if (frame == 2) {
                ctx.world.destroy(owner);
                ctx.maps().finish_world_loading();
                CHECK_FALSE(hit(32));
                owner = create_terrain(ctx);
                ctx.world.get<Transform>(focus)->position.x = 288;
                ctx.maps().finish_world_loading();
                CHECK_FALSE(hit(288));
            }
            if (frame == 3) {
                ctx.world.get<Transform>(focus)->position.x = 32;
                ctx.maps().finish_world_loading();
                const auto valid = hit(32);
                REQUIRE(valid);
                CHECK(entity_of(*valid) == owner);
                test::write_bytes(dir / "late.aheight", payload);
            }
            if (frame == 4) {
                ctx.world.get<Transform>(focus)->position.x = 288;
                ctx.maps().finish_world_loading();
                CHECK_FALSE(hit(32));
                REQUIRE(hit(288));
            }
        }
    } game;
    game.dir = dir;
    game.payload = write_terrain_tile(heights);
    const auto root = dir.string();
    App app({.headless = true, .fixed_timestep = 1.0f / 60, .worker_threads = 2, .asset_root = root, .hot_reload = false});
    app.run_frames(game, 4);
}
