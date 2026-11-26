#include <astral/world/map_state.hpp>
#include <astral/world/transform.hpp>

#include <doctest/doctest.h>

using namespace astral;

namespace {

Json parse(std::string_view text) {
    std::string error;
    auto j = Json::parse(text, &error);
    REQUIRE_MESSAGE(j.has_value(), error);
    return *j;
}

struct Fixture {
    TypeRegistry types;
    PrefabLibrary prefabs{types, [](std::string_view) { return SourceDocument{.error = "no prefabs here"}; }};
    World world;
    MapDesc map;
    std::vector<MapEntity> cell;
    const CellKey key{0, 0, 0};

    Fixture() {
        register_world_types(types);
        map.grids.emplace_back();
        map.layers.push_back({"night", false});
        auto add = [&](std::uint64_t id, const char* name, float x, std::vector<std::string> layers) {
            MapEntity e;
            e.id = id;
            e.record = parse(R"({"id":)" + std::to_string(id) + R"(,"components":{"astral::Name":{"value":")" + name +
                             R"("},"astral::Transform":{"position":[)" + std::to_string(x) + R"(,0,0]}}})");
            e.layers = std::move(layers);
            cell.push_back(std::move(e));
        };
        add(1, "rock", 1, {});
        add(2, "door", 2, {});
        add(3, "lantern", 3, {"night"});
    }

    std::size_t members() {
        std::size_t n = 0;
        world.each<const CellMember>([&](Entity, const CellMember& m) { n += m.cell == key ? 1 : 0; });
        return n;
    }
};

} // namespace

TEST_CASE("MapLayers starts from the map and knows which entities are visible") {
    Fixture f;
    MapLayers layers(f.map);
    CHECK_FALSE(layers.active("night"));
    CHECK(layers.visible(f.cell[0]));
    CHECK_FALSE(layers.visible(f.cell[2]));
    CHECK(layers.set("night", true));
    CHECK(layers.visible(f.cell[2]));
    CHECK_FALSE(layers.set("unknown", true));
    CHECK(layers.version() == 1);
}

TEST_CASE("spawn_cell spawns visible entities with CellMember; despawn_cell removes them") {
    Fixture f;
    MapLayers layers(f.map);
    MapState state;
    const LoadResult r = state.spawn_cell(f.world, f.prefabs, f.key, f.cell, layers);
    CHECK(r.errors.empty());
    CHECK(r.entities.size() == 2);
    CHECK(f.members() == 2);
    CHECK(find_by_name(f.world, "rock").valid());
    CHECK_FALSE(find_by_name(f.world, "lantern").valid());

    state.despawn_cell(f.world, f.prefabs, f.key);
    CHECK(f.members() == 0);
    CHECK_FALSE(find_by_name(f.world, "rock").valid());
    CHECK(state.diff(f.key) == nullptr); // nothing changed: nothing remembered
}

TEST_CASE("what the game changed survives unloading and loading the cell again") {
    Fixture f;
    MapLayers layers(f.map);
    MapState state;
    state.spawn_cell(f.world, f.prefabs, f.key, f.cell, layers);
    f.world.destroy(find_by_name(f.world, "rock"));
    f.world.get<Transform>(find_by_name(f.world, "door"))->position.y = 5.0f; // door opened upwards

    state.despawn_cell(f.world, f.prefabs, f.key);
    const CellDiff* diff = state.diff(f.key);
    REQUIRE(diff != nullptr);
    CHECK(diff->removed == std::vector<std::uint64_t>{1});
    CHECK(diff->changed.size() == 1);

    state.spawn_cell(f.world, f.prefabs, f.key, f.cell, layers);
    CHECK_FALSE(find_by_name(f.world, "rock").valid());
    const Entity door = find_by_name(f.world, "door");
    REQUIRE(door.valid());
    CHECK(f.world.get<Transform>(door)->position.y == doctest::Approx(5.0f));
    CHECK(f.world.get<Transform>(door)->position.x == doctest::Approx(2.0f));
}

TEST_CASE("switching a layer spawns or removes its entities in loaded cells") {
    Fixture f;
    MapLayers layers(f.map);
    MapState state;
    state.spawn_cell(f.world, f.prefabs, f.key, f.cell, layers);
    layers.set("night", true);
    state.refresh_layers(f.world, f.prefabs, layers);
    CHECK(find_by_name(f.world, "lantern").valid());
    CHECK(f.members() == 3);
    layers.set("night", false);
    state.refresh_layers(f.world, f.prefabs, layers);
    CHECK_FALSE(find_by_name(f.world, "lantern").valid());
    CHECK(f.members() == 2);
    // A hidden entity is not "removed by the game".
    state.despawn_cell(f.world, f.prefabs, f.key);
    CHECK(state.diff(f.key) == nullptr);
}

TEST_CASE("hiding a layer keeps what the game changed on its entities") {
    Fixture f;
    MapLayers layers(f.map);
    MapState state;
    state.spawn_cell(f.world, f.prefabs, f.key, f.cell, layers);
    layers.set("night", true);
    state.refresh_layers(f.world, f.prefabs, layers);
    f.world.get<Transform>(find_by_name(f.world, "lantern"))->position.y = 7.0f; // the game moved it

    SUBCASE("shown again in the same load") {
        layers.set("night", false);
        state.refresh_layers(f.world, f.prefabs, layers);
        layers.set("night", true);
        state.refresh_layers(f.world, f.prefabs, layers);
        const Entity lantern = find_by_name(f.world, "lantern");
        REQUIRE(lantern.valid());
        CHECK(f.world.get<Transform>(lantern)->position.y == doctest::Approx(7.0f));
    }
    SUBCASE("hidden, then the cell unloads and loads again") {
        layers.set("night", false);
        state.refresh_layers(f.world, f.prefabs, layers);
        state.despawn_cell(f.world, f.prefabs, f.key);
        const CellDiff* diff = state.diff(f.key);
        REQUIRE(diff != nullptr);
        CHECK(diff->changed.size() == 1);
        layers.set("night", true);
        state.spawn_cell(f.world, f.prefabs, f.key, f.cell, layers);
        const Entity lantern = find_by_name(f.world, "lantern");
        REQUIRE(lantern.valid());
        CHECK(f.world.get<Transform>(lantern)->position.y == doctest::Approx(7.0f));
    }
}

TEST_CASE("an entity the game destroyed stays destroyed when its layer is hidden and shown") {
    Fixture f;
    MapLayers layers(f.map);
    MapState state;
    state.spawn_cell(f.world, f.prefabs, f.key, f.cell, layers);
    layers.set("night", true);
    state.refresh_layers(f.world, f.prefabs, layers);
    f.world.destroy(find_by_name(f.world, "lantern"));
    layers.set("night", false);
    state.refresh_layers(f.world, f.prefabs, layers);
    layers.set("night", true);
    state.refresh_layers(f.world, f.prefabs, layers);
    CHECK_FALSE(find_by_name(f.world, "lantern").valid());
    state.despawn_cell(f.world, f.prefabs, f.key);
    const CellDiff* diff = state.diff(f.key);
    REQUIRE(diff != nullptr);
    CHECK(diff->removed == std::vector<std::uint64_t>{3});
}
