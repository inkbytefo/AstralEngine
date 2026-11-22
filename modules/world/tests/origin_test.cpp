#include <astral/world/map_state.hpp>
#include <astral/world/origin.hpp>
#include <astral/world/transform.hpp>

#include <doctest/doctest.h>

using namespace astral;

TEST_CASE("FloatingOrigin shifts when the focus leaves the threshold, snapped") {
    FloatingOrigin origin(2000.0f, 100.0f);
    CHECK_FALSE(origin.update({1999, 0, 0}).has_value());
    auto shift = origin.update({2049, 3, -10});
    REQUIRE(shift.has_value());
    CHECK(shift->x == doctest::Approx(2000.0f)); // snapped to 100 m
    CHECK(shift->y == doctest::Approx(0.0f));
    CHECK(shift->z == doctest::Approx(0.0f));
    CHECK(origin.origin().x == doctest::Approx(2000.0));
    // After the shift the focus is local (49, 3, -10); world position is unchanged.
    const WorldPosition w = origin.to_world({49, 3, -10});
    CHECK(w.x == doctest::Approx(2049.0));
    const Vec3 back = origin.to_local({2049.0, 3.0, -10.0});
    CHECK(back.x == doctest::Approx(49.0f));
    CHECK(back.z == doctest::Approx(-10.0f));
}

TEST_CASE("far from zero, local positions stay small and precise") {
    FloatingOrigin origin(2000.0f, 100.0f);
    Vec3 focus{0, 0, 0};
    double world_x = 0.0;
    for (int i = 0; i < 100; ++i) { // fly 100 km in 1 km steps
        focus.x += 1000.0f;
        world_x += 1000.0;
        if (auto s = origin.update(focus)) focus.x -= s->x;
    }
    CHECK(origin.to_world(focus).x == doctest::Approx(world_x));
    CHECK(focus.x < 2001.0f);
    const Vec3 near = origin.to_local({world_x + 0.001, 0, 0});
    CHECK(near.x - focus.x == doctest::Approx(0.001f).epsilon(0.01)); // millimetres survive at 100 km
}

TEST_CASE("shift_roots moves top-level transforms only") {
    World world;
    const Entity root = world.create(Transform{.position = {2005, 1, 0}});
    const Entity child = world.create(Transform{.position = {1, 0, 0}}, Parent{root});
    shift_roots(world, {2000, 0, 0});
    CHECK(world.get<Transform>(root)->position.x == doctest::Approx(5.0f));
    CHECK(world.get<Transform>(child)->position.x == doctest::Approx(1.0f));
}

TEST_CASE("map cells spawn relative to the origin and remember world positions") {
    TypeRegistry types;
    register_world_types(types);
    PrefabLibrary prefabs(types, [](std::string_view) { return SourceDocument{.error = "none"}; });
    World world;
    MapDesc map;
    map.grids.emplace_back();
    MapLayers layers(map);
    std::string error;
    MapEntity e;
    e.id = 1;
    e.record = *Json::parse(R"({"id":1,"components":{"astral::Name":{"value":"tower"},
                                "astral::Transform":{"position":[50000.25,0,0]}}})", &error);
    const CellKey key{0, 390, 0};

    MapState state;
    state.set_origin({50000.0, 0.0, 0.0});
    state.spawn_cell(world, prefabs, key, std::span(&e, 1), layers);
    const Entity tower = find_by_name(world, "tower");
    REQUIRE(tower.valid());
    CHECK(world.get<Transform>(tower)->position.x == doctest::Approx(0.25f)); // exact, not 50000.25f - 50000

    // The origin moves while the cell is loaded: nothing counts as changed.
    shift_roots(world, {-1000, 0, 0});
    state.set_origin({49000.0, 0.0, 0.0});
    state.despawn_cell(world, prefabs, key);
    CHECK(state.diff(key) == nullptr);

    // A real move is remembered in world coordinates.
    state.spawn_cell(world, prefabs, key, std::span(&e, 1), layers);
    const Entity again = find_by_name(world, "tower");
    CHECK(world.get<Transform>(again)->position.x == doctest::Approx(1000.25f));
    world.get<Transform>(again)->position.x += 10.0f;
    state.despawn_cell(world, prefabs, key);
    REQUIRE(state.diff(key) != nullptr);
    const Json& p = state.diff(key)->changed.at(0)["components"]["astral::Transform"]["position"];
    CHECK(p[0].as_number() == doctest::Approx(50010.25).epsilon(1e-7));
}
