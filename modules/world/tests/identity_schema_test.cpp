#include <astral/world/map_state.hpp>
#include <astral/world/scene.hpp>
#include <astral/world/serialize.hpp>
#include <astral/world/world.hpp>
#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace astral;

namespace {

struct Health {
    int hp = 100;
    int max_hp = 100;
};

struct Link {
    Entity target{};
};

} // namespace

ASTRAL_REFLECT(Health, hp, max_hp)
ASTRAL_REFLECT(Link, target)

TEST_CASE("WorldId and WorldEntityRef detect cross-world handle misuse") {
    World a;
    World b;
    CHECK(a.id() != 0);
    CHECK(b.id() != 0);
    CHECK(a.id() != b.id());

    const Entity ea = a.create();
    const Entity eb = b.create();
    // Both worlds hand out slot (0, gen 1), so raw Entity values match, but WorldEntityRef distinguishes them.
    CHECK(ea == eb);
    const WorldEntityRef ra = a.ref(ea);
    const WorldEntityRef rb = b.ref(eb);
    CHECK(ra != rb);
    CHECK(a.owns(ra));
    CHECK_FALSE(a.owns(rb));
    CHECK(b.owns(rb));
    CHECK_FALSE(b.owns(ra));

    a.destroy(ea);
    CHECK_FALSE(a.owns(ra));
}

TEST_CASE("live_save_context and live_load_context reject cross-world entity references") {
    World a;
    World b;
    TypeRegistry types;
    types.add<Link>();
    const TypeRegistry::Entry* entry = types.find("Link");
    REQUIRE(entry != nullptr);

    const Entity target_a = a.create();
    const Entity holder_a = a.create(Link{target_a});
    const Entity holder_b = b.create();
    (void)b.create(); // occupy slot (0, 1) in world b

    const Json saved = component_to_json(a, *entry, holder_a);
    REQUIRE(saved.is_object());
    CHECK(saved["target"].as_string().starts_with("#w"));

    // Round-trip inside world a succeeds.
    const Entity holder_a2 = a.create();
    CHECK(component_from_json(a, *entry, holder_a2, saved).empty());
    CHECK(a.get<const Link>(holder_a2)->target == target_a);

    // Loading world a's live reference into world b reports an error and clears target.
    const auto errors = component_from_json(b, *entry, holder_b, saved);
    CHECK_FALSE(errors.empty());
    CHECK_FALSE(b.get<const Link>(holder_b)->target.valid());
}

TEST_CASE("DocumentEntityId parses and formats top-level and prefab part addresses") {
    const auto top = DocumentEntityId::parse("42");
    REQUIRE(top.has_value());
    CHECK(top->record_id == 42);
    CHECK(top->part_path.empty());
    CHECK(top->to_string() == "42");

    const auto part = DocumentEntityId::parse("2/5/9");
    REQUIRE(part.has_value());
    CHECK(part->record_id == 2);
    CHECK(part->part_path == "5/9");
    CHECK(part->to_string() == "2/5/9");

    CHECK_FALSE(DocumentEntityId::parse("").has_value());
    CHECK_FALSE(DocumentEntityId::parse("0").has_value());
    CHECK_FALSE(DocumentEntityId::parse("2/").has_value());
    CHECK_FALSE(DocumentEntityId::parse("abc").has_value());
}

TEST_CASE("TypeRegistry schema versioning migrates older data and rejects newer schemas") {
    TypeRegistry types;
    types.add<Health>();
    REQUIRE(types.set_schema_version<Health>(2, [](std::uint32_t from, Json& v, std::string&) {
        if (from == 1) {
            if (const Json* legacy = v.find("value")) {
                Json hp_val = *legacy;
                v["hp"] = std::move(hp_val);
            }
            if (!v.find("max_hp")) {
                if (const Json* hp = v.find("hp")) {
                    Json max_val = *hp;
                    v["max_hp"] = std::move(max_val);
                }
            }
            return true;
        }
        return false;
    }));

    // Loading v1 document (implicit schema_version = 1) migrates legacy `value` -> `hp` and `max_hp`.
    World w;
    const auto v1_doc = Json::parse(R"({
        "format": "astral.world",
        "version": 1,
        "entities": [{"id": 0, "components": {"Health": {"value": 75}}}]
    })");
    REQUIRE(v1_doc.has_value());
    const LoadResult loaded = load_world(w, types, *v1_doc);
    CHECK(loaded.errors.empty());
    REQUIRE(loaded.entities.size() == 1);
    const Health* h = w.get<const Health>(loaded.entities[0]);
    REQUIRE(h != nullptr);
    CHECK(h->hp == 75);
    CHECK(h->max_hp == 75);

    // Saving a v2 component stamps "$schema_version": 2.
    const Json saved = save_world(w, types);
    CHECK(saved["entities"][0]["components"]["Health"]["$schema_version"].as_number() == 2.0);

    // Future v3 document produces a structured diagnostic and skips loading.
    World w2;
    const auto v3_doc = Json::parse(R"({
        "format": "astral.world",
        "version": 1,
        "entities": [{"id": 0, "components": {"Health": {"$schema_version": 3, "hp": 50}}}]
    })");
    REQUIRE(v3_doc.has_value());
    const LoadResult future = load_world(w2, types, *v3_doc);
    CHECK_FALSE(future.errors.empty());
    CHECK_FALSE(w2.has<Health>(future.entities[0]));
}

TEST_CASE("MapState tracks Unknown, Unloaded, Alive, Reloaded, and Destroyed map entity states") {
    World world;
    TypeRegistry types;
    register_world_types(types);
    PrefabLibrary prefabs(types, [](std::string_view) { return SourceDocument{}; });

    MapDesc map;
    map.grids.push_back(GridDesc{.name = "main", .cell_size = 100.0, .load_range = 200.0});
    MapLayers layers(map);
    MapState state;

    const CellKey cell{0, 0, 0};
    CHECK(state.lookup(world, 77).status == MapEntityStatus::Unknown);
    state.register_known(77, cell);
    CHECK(state.lookup(world, 77).status == MapEntityStatus::Unloaded);

    MapEntity me;
    me.id = 77;
    me.grid = "main";
    me.position = {10.0, 0.0, 10.0};
    me.record = *Json::parse(R"({"id":77,"components":{"astral::Name":{"value":"crate"},"astral::Transform":{"position":[10,0,10]}}})");

    const std::vector<MapEntity> list{me};
    state.spawn_cell(world, prefabs, cell, list, layers);
    const MapEntityLookup first = state.lookup(world, 77);
    CHECK(first.status == MapEntityStatus::Alive);
    CHECK(first.load_count == 1);
    CHECK(world.alive(first.entity));

    // Unload then reload: load_count increments to 2.
    state.despawn_cell(world, prefabs, cell);
    CHECK(state.lookup(world, 77).status == MapEntityStatus::Unloaded);
    CHECK_FALSE(world.alive(first.entity));

    state.spawn_cell(world, prefabs, cell, list, layers);
    const MapEntityLookup reloaded = state.lookup(world, 77);
    CHECK(reloaded.status == MapEntityStatus::Alive);
    CHECK(reloaded.load_count == 2);
    CHECK(world.alive(reloaded.entity));

    // Destroy entity while loaded and despawn cell -> permanently Destroyed.
    world.destroy(reloaded.entity);
    CHECK(state.lookup(world, 77).status == MapEntityStatus::Destroyed);
    state.despawn_cell(world, prefabs, cell);
    CHECK(state.lookup(world, 77).status == MapEntityStatus::Destroyed);
}

TEST_CASE("Commands apply add, remove, re-add, and destroy in deterministic submission order") {
    World world;
    const Entity e = world.create();
    world.commands().add(e, Health{10, 10});
    world.commands().remove<Health>(e);
    world.commands().add(e, Health{42, 50});
    world.apply_commands();
    REQUIRE(world.has<Health>(e));
    CHECK(world.get<const Health>(e)->hp == 42);

    world.commands().destroy(e);
    world.commands().add(e, Health{99, 99}); // dead after destroy -> ignored
    world.apply_commands();
    CHECK_FALSE(world.alive(e));
}
