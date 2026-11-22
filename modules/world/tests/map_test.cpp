#include <astral/world/map.hpp>

#include <doctest/doctest.h>

using namespace astral;

namespace {

Json parse(std::string_view text) {
    std::string error;
    auto j = Json::parse(text, &error);
    REQUIRE_MESSAGE(j.has_value(), error);
    return *j;
}

EntityFile ent(std::string path, std::string_view text) {
    return {std::move(path), parse(text)};
}

const char* kMap = R"({
  "format": "astral.world", "version": 1,
  "settings": { "gravity": -9.81 },
  "grids": [ { "name": "main", "cell_size": 100, "load_range": 300 },
             { "name": "landmarks", "cell_size": 500, "load_range": 2000 } ],
  "layers": [ { "name": "night", "active": false } ]
})";

} // namespace

TEST_CASE("parse_map reads settings, grids and layers") {
    std::string error;
    auto map = parse_map(parse(kMap), &error);
    REQUIRE_MESSAGE(map.has_value(), error);
    CHECK(map->settings["gravity"].as_number() == doctest::Approx(-9.81));
    REQUIRE(map->grids.size() == 2);
    CHECK(map->grids[0].name == "main");
    CHECK(map->grids[0].cell_size == doctest::Approx(100.0));
    CHECK(map->grids[1].load_range == doctest::Approx(2000.0));
    REQUIRE(map->layers.size() == 1);
    CHECK(map->layers[0].name == "night");
    CHECK_FALSE(map->layers[0].active);
}

TEST_CASE("parse_map adds a default main grid and refuses other formats") {
    auto map = parse_map(parse(R"({"format":"astral.world","version":1})"));
    REQUIRE(map.has_value());
    REQUIRE(map->grids.size() == 1);
    CHECK(map->grids[0].name == "main");
    CHECK(map->grids[0].cell_size == doctest::Approx(128.0));
    CHECK(map->grids[0].load_range == doctest::Approx(384.0));

    std::string error;
    CHECK_FALSE(parse_map(parse(R"({"format":"astral.scene","version":1})"), &error).has_value());
    CHECK_FALSE(error.empty());
    CHECK_FALSE(parse_map(parse(R"({"format":"astral.world","version":99})")).has_value());
}

TEST_CASE("write_map round-trips") {
    auto map = parse_map(parse(kMap));
    REQUIRE(map.has_value());
    auto again = parse_map(write_map(*map));
    REQUIRE(again.has_value());
    CHECK(again->grids.size() == 2);
    CHECK(again->grids[1].name == "landmarks");
    CHECK(again->layers[0].name == "night");
    CHECK(again->settings == map->settings);
}

TEST_CASE("read_map_source collects entity files sorted by id and reports problems") {
    auto map = parse_map(parse(kMap));
    REQUIRE(map.has_value());
    std::vector<EntityFile> files;
    files.push_back(ent("b.ent", R"({"format":"astral.entity","version":1,
        "entity":{"id":7,"components":{"astral::Transform":{"position":[250.5,0,-10]}}},
        "layers":["night"]})"));
    files.push_back(ent("a.ent", R"({"format":"astral.entity","version":1,
        "entity":{"id":3,"prefab":"prefabs/tree.prefab"}, "grid":"landmarks", "always_loaded":true})"));
    files.push_back(ent("dup.ent", R"({"format":"astral.entity","version":1,"entity":{"id":7}})"));
    files.push_back(ent("bad.ent", R"({"format":"astral.scene","version":1})"));
    files.push_back(ent("noid.ent", R"({"format":"astral.entity","version":1,"entity":{}})"));
    files.push_back(ent("badgrid.ent", R"({"format":"astral.entity","version":1,"entity":{"id":9},"grid":"nope"})"));

    const MapSource source = read_map_source(*map, files);
    REQUIRE(source.entities.size() == 3);
    CHECK(source.entities[0].id == 3);
    CHECK(source.entities[0].grid == "landmarks");
    CHECK(source.entities[0].always_loaded);
    CHECK(source.entities[0].record["prefab"].as_string() == "prefabs/tree.prefab");
    CHECK(source.entities[1].id == 7);
    CHECK(source.entities[1].grid == "main");
    CHECK(source.entities[1].position[0] == doctest::Approx(250.5));
    CHECK(source.entities[1].position[2] == doctest::Approx(-10.0));
    REQUIRE(source.entities[1].layers.size() == 1);
    CHECK(source.entities[1].layers[0] == "night");
    CHECK(source.entities[2].id == 9);
    CHECK(source.entities[2].grid == "main"); // unknown grid falls back (reported)
    CHECK(source.errors.size() == 4);         // duplicate id, wrong format, missing id, unknown grid
}

TEST_CASE("make_entity_file round-trips through read_map_source") {
    auto map = parse_map(parse(kMap));
    REQUIRE(map.has_value());
    MapEntity e;
    e.id = 42;
    e.record = parse(R"({"id":42,"components":{"astral::Name":{"value":"house"}}})");
    e.grid = "landmarks";
    e.layers = {"night"};
    const EntityFile file{entity_file_name(42), make_entity_file(e)};
    CHECK(file.path == "42.ent");
    const MapSource source = read_map_source(*map, std::span(&file, 1));
    REQUIRE(source.entities.size() == 1);
    CHECK(source.errors.empty());
    CHECK(source.entities[0].grid == "landmarks");
    CHECK(source.entities[0].record == e.record);
}

TEST_CASE("assign_cells puts entities into grid cells by position") {
    auto map = parse_map(parse(kMap));
    REQUIRE(map.has_value());
    std::vector<MapEntity> entities(5);
    entities[0].id = 1; entities[0].grid = "main"; entities[0].position = {50, 0, 50};      // cell 0,0
    entities[1].id = 2; entities[1].grid = "main"; entities[1].position = {-0.5, 0, 199.9}; // cell -1,1
    entities[2].id = 3; entities[2].grid = "main"; entities[2].position = {100, 0, 0};      // cell 1,0 (edge)
    entities[3].id = 4; entities[3].grid = "landmarks"; entities[3].position = {-501, 0, 10}; // landmarks -2,0
    entities[4].id = 5; entities[4].grid = "main"; entities[4].always_loaded = true;

    const CellAssignment cells = assign_cells(*map, entities);
    CHECK(cells.always_loaded == std::vector<std::size_t>{4});
    REQUIRE(cells.cells.size() == 4);
    // Sorted: by grid index, then z, then x.
    CHECK(cells.cells[0].key == CellKey{0, 0, 0});
    CHECK(cells.cells[1].key == CellKey{0, 1, 0});
    CHECK(cells.cells[2].key == CellKey{0, -1, 1});
    CHECK(cells.cells[3].key == CellKey{1, -2, 0});
    CHECK(cells.cells[3].entities == std::vector<std::size_t>{3});
    CHECK(cell_name(*map, cells.cells[2].key) == "main_-1_1");
}

TEST_CASE("cell bounds and distance") {
    auto map = parse_map(parse(kMap));
    REQUIRE(map.has_value());
    const CellKey key{0, -1, 2};
    CHECK(cell_min_x(*map, key) == doctest::Approx(-100.0));
    CHECK(cell_min_z(*map, key) == doctest::Approx(200.0));
    // Distance from a point to the cell's square on the ground plane: inside → 0.
    CHECK(cell_distance(*map, key, -50, 250) == doctest::Approx(0.0));
    CHECK(cell_distance(*map, key, 30, 250) == doctest::Approx(30.0));
    CHECK(cell_distance(*map, key, 3, 304) == doctest::Approx(5.0)); // 3-4-5 from the corner (0,300)
}

TEST_CASE("cook_map writes an index and one document per cell") {
    auto map = parse_map(parse(kMap));
    REQUIRE(map.has_value());
    std::vector<EntityFile> files;
    files.push_back(ent("1.ent", R"({"format":"astral.entity","version":1,
        "entity":{"id":1,"components":{"astral::Transform":{"position":[10,0,10]}}}})"));
    files.push_back(ent("2.ent", R"({"format":"astral.entity","version":1,
        "entity":{"id":2,"components":{"astral::Transform":{"position":[20,0,20]}}},"layers":["night"]})"));
    files.push_back(ent("3.ent", R"({"format":"astral.entity","version":1,
        "entity":{"id":3,"components":{"astral::Transform":{"position":[-300,0,0]}}}})"));
    files.push_back(ent("4.ent", R"({"format":"astral.entity","version":1,"entity":{"id":4},"always_loaded":true})"));
    const MapSource source = read_map_source(*map, files);

    const CookedMap cooked = cook_map(source);
    REQUIRE(cooked.cells.size() == 3); // always + two grid cells
    CHECK(cooked.cells[0].name == "always");
    CHECK(cooked.cells[1].name == "main_-3_0");
    CHECK(cooked.cells[2].name == "main_0_0");
    CHECK(cooked.cells[2].document["format"].as_string() == "astral.cell");
    REQUIRE(cooked.cells[2].document["entities"].size() == 2);
    CHECK(cooked.cells[2].document["entities"][1]["layers"][0].as_string() == "night");

    // The index is a valid map document plus the list of cells.
    auto again = parse_map(cooked.index);
    REQUIRE(again.has_value());
    const Json& cells = cooked.index["cells"];
    REQUIRE(cells.size() == 2);
    CHECK(cells[0]["name"].as_string() == "main_-3_0");
    CHECK(cells[0]["grid"].as_number() == 0);
    CHECK(cells[0]["x"].as_number() == -3);
    CHECK(cells[0]["z"].as_number() == 0);
    CHECK(cells[0]["entities"].as_number() == 1);
    CHECK(cooked.index["always_loaded"].as_number() == 1);

    // Cooked cells read back into entities.
    std::vector<MapEntity> back = read_cell(*again, cooked.cells[2].document);
    REQUIRE(back.size() == 2);
    CHECK(back[1].id == 2);
    CHECK(back[1].layers == std::vector<std::string>{"night"});
    CHECK(back[0].position[0] == doctest::Approx(10.0));
}
