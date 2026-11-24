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

TEST_CASE("a scene creates static, dynamic and sensor bodies and saves their authored settings") {
    const auto dir = test::temp_dir("app_physics_scene");
    write_text(dir / "level.scene", R"({"format":"astral.scene","version":1,"entities":[
      {"id":1,"components":{"astral::Transform":{"position":[0,-0.5,0]},
        "astral::RigidBody":{"desc":{"motion":"Static","shape":{"type":"Box","half_extents":[5,0.5,5]},"surface":3}}}},
      {"id":2,"components":{"astral::Transform":{"position":[0,3,0]},
        "astral::RigidBody":{"desc":{"mass":2,"shape":{"type":"Sphere","radius":0.25}}}}},
      {"id":3,"components":{"astral::Transform":{"position":[2,1,0]},
        "astral::RigidBody":{"desc":{"motion":"Static","sensor":true,"layer":2}}}}
    ]})");

    struct SceneGame final : Game {
        LoadResult loaded;
        BodyHandle destroyed_body{};
        int frames = 0;
        void on_start(Context& ctx) override { loaded = ctx.scenes().load_scene("level.scene"); }
        void on_update(Context& ctx) override {
            if (++frames != 21 || loaded.entities.size() != 3) return;
            const Entity ball = loaded.entities[1];
            const auto* body = ctx.world.get<const RigidBody>(ball);
            REQUIRE(body);
            destroyed_body = body->handle;
            REQUIRE(ctx.physics->valid(destroyed_body));
            CHECK(ctx.world.get<const Transform>(ball)->position.y < 3.0f);
            REQUIRE(ctx.scenes().save_scene("saved.scene"));
            ctx.world.destroy(ball);
        }
    } game;
    const std::string root = dir.string();
    App app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = root, .hot_reload = false});
    app.run_frames(game, 22);
    REQUIRE(game.loaded.errors.empty());
    REQUIRE(game.loaded.entities.size() == 3);
    REQUIRE(app.context().physics);
    const Entity floor = game.loaded.entities[0];
    const Entity sensor = game.loaded.entities[2];
    const auto* ground = app.world().get<const RigidBody>(floor);
    const auto* trigger = app.world().get<const RigidBody>(sensor);
    REQUIRE(ground);
    REQUIRE(trigger);
    CHECK(app.context().physics->valid(ground->handle));
    CHECK(app.context().physics->valid(trigger->handle));
    CHECK(ground->desc.motion == MotionType::Static);
    CHECK(ground->desc.surface == 3);
    CHECK(trigger->desc.sensor);
    CHECK(trigger->desc.layer == 2);
    auto saved = Json::parse(read_text(dir / "saved.scene"));
    REQUIRE(saved);
    const auto& body = (*saved)["entities"][1]["components"]["astral::RigidBody"];
    CHECK(body["desc"]["mass"].as_number() == 2.0);
    CHECK(body["desc"]["shape"]["type"].as_string() == "Sphere");
    CHECK(body["desc"]["shape"]["radius"].as_number() == 0.25);
    CHECK(body["handle"].is_null());
    CHECK(body["desc"]["cooked_shape"].is_null());

    REQUIRE(game.destroyed_body.valid());
    CHECK_FALSE(app.context().physics->valid(game.destroyed_body));

    struct ReloadGame final : Game {
        void on_start(Context& ctx) override {
            const auto loaded = ctx.scenes().load_scene("saved.scene");
            REQUIRE(loaded.errors.empty());
            REQUIRE(loaded.entities.size() == 3);
            const auto* body = ctx.world.get<const RigidBody>(loaded.entities[1]);
            REQUIRE(body);
            CHECK_FALSE(body->handle.valid());
            CHECK(body->desc.shape.type == ShapeType::Sphere);
            CHECK(body->desc.shape.radius == doctest::Approx(0.25f));
            CHECK(body->desc.mass == doctest::Approx(2.0f));
        }
    } reloaded;
    App second({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = root, .hot_reload = false});
    second.run_frames(reloaded, 1);
}

TEST_CASE("a scene character responds to Luau at the same fixed steps at different frame rates") {
    const auto dir = test::temp_dir("app_character_scene");
    write_text(dir / "level.scene", R"({"format":"astral.scene","version":1,"entities":[
      {"id":1,"components":{"astral::Transform":{"position":[0,-0.5,0]},
        "astral::RigidBody":{"desc":{"motion":"Static","shape":{"half_extents":[20,0.5,20]}}}}},
      {"id":2,"components":{"astral::Name":{"value":"player"},"astral::Transform":{"position":[0,0.1,0]},
        "astral::CharacterController":{"desc":{"radius":0.35,"height":1.7},"jump_speed":5}}}
    ]})");
    write_text(dir / "move.luau", R"(
        local tick = 0
        local launched = false
        local landed = false
        astral.system {
            phase = "FixedUpdate",
            run = function()
                tick += 1
                local player = assert(astral.find("player"))
                local state = assert(astral.physics.character_state(player))
                if tick == 20 then assert(state.on_ground) end
                if state.velocity.y > 1 then launched = true end
                if launched and state.on_ground then landed = true end
                if tick == 119 then assert(launched and landed) end
                local speed = if tick < 40 then 2 else 0
                assert(astral.physics.move_character(player, vector.create(speed, 0, 0), tick == 21))
            end
        }
    )");

    struct CharacterGame final : Game {
        void on_start(Context& ctx) override {
            REQUIRE(ctx.scenes().load_scene("level.scene").errors.empty());
            REQUIRE(ctx.script_service().load_script("move.luau").empty());
        }
    };

    const std::string root = dir.string();
    const auto simulate = [&](float delta, unsigned frames) {
        CharacterGame game;
        App app({.headless = true, .fixed_timestep = delta, .asset_root = root, .hot_reload = false});
        app.run_frames(game, frames);
        CHECK(app.context().scripts->take_errors().empty());
        CHECK(app.context().physics_service().query_state().step_index == 120);
        const Entity entity = find_by_name(app.world(), "player");
        const auto* controller = app.world().get<const CharacterController>(entity);
        REQUIRE(controller);
        CHECK(controller->desc.radius == doctest::Approx(0.35));
        CHECK(controller->on_ground);
        CHECK_FALSE(controller->jump);
        REQUIRE(app.context().scenes().save_scene("character_saved.scene"));
        const auto saved = Json::parse(read_text(dir / "character_saved.scene"));
        REQUIRE(saved);
        const auto& character = (*saved)["entities"][1]["components"]["astral::CharacterController"];
        CHECK(character["jump_speed"].as_number() == 5.0);
        CHECK(character["move"].is_null());
        CHECK(character["on_ground"].is_null());
        CHECK(character["handle"].is_null());
        return app.world().get<const Transform>(entity)->position;
    };
    const Vec3 at_60hz = simulate(1.0f / 60.0f, 120);
    const Vec3 at_30hz = simulate(1.0f / 30.0f, 60);
    CHECK(at_60hz.x > 0.5f);
    CHECK(at_60hz.x == doctest::Approx(at_30hz.x));
    CHECK(at_60hz.y == doctest::Approx(at_30hz.y));
}
