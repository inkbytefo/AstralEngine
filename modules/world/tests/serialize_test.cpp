#include <astral/world/serialize.hpp>
#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <vector>

using namespace astral;

namespace game {
enum class Team : std::uint8_t { Red, Blue };
struct Weapon {
    std::string name = "sword";
    float damage = 10;
};
struct Fighter {
    Team team = Team::Red;
    int health = 100;
    std::vector<Weapon> weapons;
    std::optional<float> shield;
    Entity target{};      // remapped on load
    Vec3 spawn{};
};
struct Boss {};
struct Runtime {
    Handle<struct GpuThingTag> gpu{}; // runtime handles are not persisted
    float value = 1;
};
struct NotRegistered {
    int x = 0;
};
} // namespace game

ASTRAL_REFLECT(game::Weapon, name, damage)
ASTRAL_REFLECT(game::Fighter, team, health, weapons, shield, target, spawn)
ASTRAL_REFLECT_EMPTY(game::Boss)
ASTRAL_REFLECT(game::Runtime, gpu, value)

namespace {
TypeRegistry make_types() {
    TypeRegistry types;
    register_world_types(types);
    types.add<game::Fighter>();
    types.add<game::Boss>();
    types.add<game::Runtime>();
    return types;
}
} // namespace

TEST_CASE("a world round-trips through JSON with entity references remapped") {
    const TypeRegistry types = make_types();
    World src;
    const Entity hero = src.create(Transform{.position = {1, 2, 3}}, game::Fighter{.team = game::Team::Blue, .health = 80, .weapons = {}, .shield = {}, .target = {}, .spawn = {}});
    const Entity boss = src.create(Transform{.position = {9, 0, 0}, .scale = {2, 2, 2}},
                                   game::Fighter{.team = game::Team::Red, .health = 100, .weapons = {{"axe", 30}, {"fist", 5}}, .shield = 50.0f, .target = hero, .spawn = {}},
                                   game::Boss{});
    src.get<game::Fighter>(hero)->target = boss;
    src.create(Transform{}, Parent{hero}, game::NotRegistered{7}); // unregistered parts are skipped
    src.create(game::NotRegistered{});                             // nothing to save: entity skipped
    src.create(game::Runtime{.gpu = {3, 1}, .value = 4});

    const Json saved = save_world(src, types);
    CHECK(saved["entities"].size() == 4);

    // Through text, like a file on disk.
    const auto text = saved.dump(2);
    const auto parsed = Json::parse(text);
    REQUIRE(parsed.has_value());

    World dst;
    dst.create(); // pre-existing entities must not confuse the mapping
    const LoadResult result = load_world(dst, types, *parsed);
    CHECK(result.errors.empty());
    REQUIRE(result.entities.size() == 4);

    const Entity hero2 = result.entities[0], boss2 = result.entities[1], child2 = result.entities[2];
    CHECK(dst.get<Transform>(hero2)->position.y == 2.0f);
    CHECK(dst.get<game::Fighter>(hero2)->team == game::Team::Blue);
    CHECK(dst.get<game::Fighter>(hero2)->target == boss2);
    CHECK(dst.get<game::Fighter>(boss2)->target == hero2);
    CHECK(dst.has<game::Boss>(boss2));
    CHECK(dst.get<Transform>(boss2)->scale.x == 2.0f);
    REQUIRE(dst.get<game::Fighter>(boss2)->weapons.size() == 2);
    CHECK(dst.get<game::Fighter>(boss2)->weapons[0].name == "axe");
    CHECK(dst.get<game::Fighter>(boss2)->weapons[1].damage == 5.0f);
    CHECK(dst.get<game::Fighter>(boss2)->shield == 50.0f);
    CHECK_FALSE(dst.get<game::Fighter>(hero2)->shield.has_value());
    CHECK(dst.get<Parent>(child2)->entity == hero2);
    CHECK_FALSE(dst.has<game::NotRegistered>(child2));
    CHECK_FALSE(dst.get<game::Runtime>(result.entities[3])->gpu.valid());
    CHECK(dst.get<game::Runtime>(result.entities[3])->value == 4.0f);

    // Saving the loaded world gives the same document: stable, diff-friendly output.
    CHECK(save_world(dst, types).dump(2) == text);
}

TEST_CASE("loading is lenient: missing fields keep defaults, problems are reported, not fatal") {
    const TypeRegistry types = make_types();
    const auto doc = Json::parse(R"({
        "format": "astral.world", "version": 1,
        "entities": [
            {"id": 0, "components": {"game::Fighter": {"health": 5, "unknown_field": 1}}},
            {"id": 1, "components": {"game::Missing": {}, "game::Fighter": {"health": "lots", "target": 99}}}
        ]})");
    REQUIRE(doc.has_value());
    World w;
    const LoadResult r = load_world(w, types, *doc);
    REQUIRE(r.entities.size() == 2);
    CHECK(w.get<game::Fighter>(r.entities[0])->health == 5);
    CHECK(w.get<game::Fighter>(r.entities[0])->team == game::Team::Red); // default kept
    CHECK(w.get<game::Fighter>(r.entities[1])->health == 100);           // bad type: default kept
    CHECK_FALSE(w.get<game::Fighter>(r.entities[1])->target.valid());    // dangling reference: null
    CHECK(r.errors.size() == 3); // unknown component, bad health, dangling target
}

TEST_CASE("documents of another format or version are refused") {
    const TypeRegistry types = make_types();
    World w;
    CHECK_FALSE(load_world(w, types, Json::parse(R"({"format": "other", "version": 1, "entities": []})").value()).errors.empty());
    CHECK_FALSE(load_world(w, types, Json::parse(R"({"format": "astral.world", "version": 99, "entities": []})").value()).errors.empty());
    CHECK(w.count() == 0);
}

TEST_CASE("the registry knows names and stable ids") {
    const TypeRegistry types = make_types();
    const auto* f = types.find("game::Fighter");
    REQUIRE(f != nullptr);
    CHECK(f->id == stable_hash("game::Fighter"));
    CHECK(types.find("astral::Transform") != nullptr); // engine types keep their namespace
    CHECK(types.find("nope") == nullptr);
    TypeRegistry again = make_types();
    again.add<game::Fighter>(); // registering twice is harmless
    CHECK(again.entries().size() == types.entries().size());
}

TEST_CASE("entities with an excluded component are not saved") {
    TypeRegistry types = make_types();
    types.exclude<game::Boss>();
    World w;
    w.create(Transform{});
    w.create(Transform{}, game::Boss{});
    CHECK(save_world(w, types)["entities"].size() == 1);
}
