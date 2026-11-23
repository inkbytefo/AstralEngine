// Unit and regression tests for E1 Level Instances, nesting/cycle diagnostics, large-instance limits,
// multi-cell ownership, cross-cell refs, override merge, and cook consistency (ADR-0036).
#include <astral/world/level_instance.hpp>
#include <astral/world/map_state.hpp>
#include <astral/world/scene.hpp>
#include <doctest/doctest.h>

#include <string>
#include <unordered_map>
#include <vector>

using namespace astral;

namespace {

struct OutpostLamp {
    float intensity = 12.0f;
};

TypeRegistry make_types() {
    TypeRegistry types;
    register_world_types(types);
    register_level_instance_types(types);
    types.add<OutpostLamp>();
    return types;
}

} // namespace

ASTRAL_REFLECT(OutpostLamp, intensity)

TEST_CASE("two independent Level Instances keep distinct ids, placement without parenting, and isolated overrides") {
    const TypeRegistry types = make_types();
    std::unordered_map<std::string, Json> docs;
    docs["scenes/outpost.scene"] = *Json::parse(R"({
        "format": "astral.scene",
        "version": 1,
        "entities": [
            {"id": 1, "components": {
                "astral::Name": {"value": "Tower"},
                "astral::Transform": {"position": [0, 0, 0]}
            }},
            {"id": 2, "components": {
                "astral::Name": {"value": "Beacon"},
                "astral::Transform": {"position": [300, 5, 0]},
                "OutpostLamp": {"intensity": 12}
            }}
        ]
    })");
    const DocumentSource loader = [&](std::string_view p) -> SourceDocument {
        auto it = docs.find(std::string(p));
        return it == docs.end() ? SourceDocument{nullptr, 0, "not found"} : SourceDocument{&it->second, 1, {}};
    };

    LevelInstancePlacement a{.instance_id = 100, .scene_path = "scenes/outpost.scene", .position = {100.0, 0.0, 100.0}};
    LevelInstancePlacement b{.instance_id = 200, .scene_path = "scenes/outpost.scene", .position = {1200.0, 0.0, 1200.0}};
    set_level_instance_override(a, "2", "OutpostLamp", "intensity", 88.0);

    const auto ea = expand_level_instance(a, types, loader);
    const auto eb = expand_level_instance(b, types, loader);
    CHECK(ea.errors.empty());
    CHECK(eb.errors.empty());
    REQUIRE(ea.entities.size() == 2);
    REQUIRE(eb.entities.size() == 2);

    // Inner entities have distinct deterministic ids and NO artificial Parent component.
    CHECK(ea.entities[0].id != eb.entities[0].id);
    CHECK(ea.entities[1].id != eb.entities[1].id);
    CHECK(ea.entities[0].record["components"].find("astral::Parent") == nullptr);
    CHECK(ea.entities[1].record["components"]["OutpostLamp"]["intensity"].as_number() == doctest::Approx(88.0));
    CHECK(eb.entities[1].record["components"]["OutpostLamp"]["intensity"].as_number() == doctest::Approx(12.0));

    // Placement transforms compose world positions without parenting.
    CHECK(ea.entities[1].position[0] == doctest::Approx(400.0));
    CHECK(eb.entities[1].position[0] == doctest::Approx(1500.0));

    // Edit-in-context source change merges with instance A's override on re-expansion (hot reload).
    docs["scenes/outpost.scene"] = *Json::parse(R"({
        "format": "astral.scene",
        "version": 1,
        "entities": [
            {"id": 1, "components": {
                "astral::Name": {"value": "Upgraded Tower"},
                "astral::Transform": {"position": [0, 2, 0]}
            }},
            {"id": 2, "components": {
                "astral::Name": {"value": "Beacon"},
                "astral::Transform": {"position": [300, 5, 0]},
                "OutpostLamp": {"intensity": 20}
            }}
        ]
    })");
    const auto ea2 = expand_level_instance(a, types, loader);
    const auto eb2 = expand_level_instance(b, types, loader);
    CHECK(ea2.entities[0].record["components"]["astral::Name"]["value"].as_string() == "Upgraded Tower");
    CHECK(eb2.entities[0].record["components"]["astral::Name"]["value"].as_string() == "Upgraded Tower");
    // A kept its override (88); B picked up the source change (20)!
    CHECK(ea2.entities[1].record["components"]["OutpostLamp"]["intensity"].as_number() == doctest::Approx(88.0));
    CHECK(eb2.entities[1].record["components"]["OutpostLamp"]["intensity"].as_number() == doctest::Approx(20.0));
}

TEST_CASE("nested Level Instances detect cycles and enforce max entity limits") {
    const TypeRegistry types = make_types();
    std::unordered_map<std::string, Json> docs;
    docs["a.scene"] = *Json::parse(R"({"format":"astral.scene","version":1,"entities":[
        {"id":1,"level_instance":{"scene":"b.scene","position":[10,0,0]}}
    ]})");
    docs["b.scene"] = *Json::parse(R"({"format":"astral.scene","version":1,"entities":[
        {"id":1,"level_instance":{"scene":"a.scene","position":[5,0,0]}}
    ]})");
    docs["big.scene"] = *Json::parse(R"({"format":"astral.scene","version":1,"entities":[
        {"id":1,"components":{"astral::Transform":{"position":[0,0,0]}}},
        {"id":2,"components":{"astral::Transform":{"position":[1,0,0]}}},
        {"id":3,"components":{"astral::Transform":{"position":[2,0,0]}}}
    ]})");
    const DocumentSource loader = [&](std::string_view p) -> SourceDocument {
        auto it = docs.find(std::string(p));
        return it == docs.end() ? SourceDocument{nullptr, 0, "missing"} : SourceDocument{&it->second, 1, {}};
    };

    const auto cyc = expand_level_instance({.instance_id = 10, .scene_path = "a.scene"}, types, loader);
    CHECK_FALSE(cyc.errors.empty());
    CHECK(cyc.errors.front().find("cycle") != std::string::npos);

    const auto lim = expand_level_instance({.instance_id = 11, .scene_path = "big.scene"}, types, loader, {.max_depth = 4, .max_entities = 2});
    CHECK(lim.truncated);
    CHECK(lim.entities.size() == 2);
    CHECK_FALSE(lim.errors.empty());
}

TEST_CASE("multi-cell Level Instance cooks consistently and preserves cross-cell refs across cell unload/reload") {
    const TypeRegistry types = make_types();
    std::unordered_map<std::string, Json> docs;
    const std::uint64_t target_in_cell_b = level_instance_entity_id(200, "1");
    docs["scenes/hub.scene"] = *Json::parse(R"({
        "format": "astral.scene",
        "version": 1,
        "entities": [
            {"id": 1, "components": {
                "astral::Name": {"value": "WestGate"},
                "astral::Transform": {"position": [10, 0, 10]}
            }},
            {"id": 2, "components": {
                "astral::Name": {"value": "EastWatch"},
                "astral::Transform": {"position": [400, 0, 10]}
            }}
        ]
    })");
    const DocumentSource loader = [&](std::string_view p) -> SourceDocument {
        auto it = docs.find(std::string(p));
        return it == docs.end() ? SourceDocument{nullptr, 0, "missing"} : SourceDocument{&it->second, 1, {}};
    };

    MapDesc map;
    map.grids.push_back(GridDesc{.name = "main", .cell_size = 256.0, .load_range = 512.0});

    LevelInstancePlacement li_a{.instance_id = 100, .scene_path = "scenes/hub.scene", .position = {0.0, 0.0, 0.0}};
    set_level_instance_override(li_a, "1", "astral::CrossCellRef", "target_id", static_cast<double>(target_in_cell_b));
    set_level_instance_override(li_a, "1", "astral::CrossCellRef", "target_label", "InstanceB/WestGate");

    LevelInstancePlacement li_b{.instance_id = 200, .scene_path = "scenes/hub.scene", .position = {1000.0, 0.0, 1000.0}};

    const std::vector<EntityFile> files{
        {"100.ent", make_level_instance_entity_file(li_a)},
        {"200.ent", make_level_instance_entity_file(li_b)},
    };
    const MapSource raw = read_map_source(map, files);
    REQUIRE(raw.errors.empty());
    const MapSource expanded = expand_map_level_instances(raw, types, loader);
    REQUIRE(expanded.errors.empty());
    REQUIRE(expanded.entities.size() == 4);

    // Multi-cell ownership: li_a's WestGate (x=10) is in cell (0,0) and EastWatch (x=400) is in cell (1,0); li_b in (3,3) and (5,3).
    const CookedMap cooked = cook_map(expanded);
    CHECK(cooked.cells.size() >= 5); // "always" + at least 4 distinct cells

    // Spawn into MapState, unload, reload, and verify persistent identities + cross-cell ref resolution survive!
    World world;
    PrefabLibrary prefabs(types, loader);
    MapLayers layers(map);
    MapState state;
    const CellAssignment assigned = assign_cells(map, expanded.entities);
    for (const Cell& c : assigned.cells) {
        for (std::size_t idx : c.entities) state.register_known(expanded.entities[idx].id, c.key);
    }

    auto cell_entities = [&](CellKey key) {
        std::vector<MapEntity> out;
        for (const CookedCell& cc : cooked.cells) {
            if (cc.name == cell_name(map, key)) return read_cell(map, cc.document);
        }
        return out;
    };

    const CellKey cell_a0{0, 0, 0};
    const CellKey cell_b0{0, 3, 3};
    state.spawn_cell(world, prefabs, cell_a0, cell_entities(cell_a0), layers);
    const std::uint64_t west_a_id = level_instance_entity_id(100, "1");
    const MapEntityLookup west_a = state.lookup(world, west_a_id);
    REQUIRE(west_a.status == MapEntityStatus::Alive);
    const auto* xref = world.get<const CrossCellRef>(west_a.entity);
    REQUIRE(xref != nullptr);
    CHECK(xref->target_id == target_in_cell_b);
    // Target cell not loaded yet -> Unloaded!
    CHECK(state.lookup(world, xref->target_id).status == MapEntityStatus::Unloaded);

    // Load target cell -> cross-cell ref resolves to Alive!
    state.spawn_cell(world, prefabs, cell_b0, cell_entities(cell_b0), layers);
    CHECK(state.lookup(world, xref->target_id).status == MapEntityStatus::Alive);

    // Unload and reload cell_a0 -> id, load_count, and CrossCellRef survive intact!
    state.despawn_cell(world, prefabs, cell_a0);
    CHECK(state.lookup(world, west_a_id).status == MapEntityStatus::Unloaded);
    state.spawn_cell(world, prefabs, cell_a0, cell_entities(cell_a0), layers);
    const MapEntityLookup west_a_reloaded = state.lookup(world, west_a_id);
    REQUIRE(west_a_reloaded.status == MapEntityStatus::Alive);
    CHECK(west_a_reloaded.load_count == 2);
    const auto* xref2 = world.get<const CrossCellRef>(west_a_reloaded.entity);
    REQUIRE(xref2 != nullptr);
    CHECK(xref2->target_id == target_in_cell_b);
    CHECK(state.lookup(world, xref2->target_id).status == MapEntityStatus::Alive);
}
