// GPU-free unit tests for E1 WorldAuthoringSession, data-driven placement tools,
// height sampling hook, Level Instance overrides vs edit-in-context, and cell ownership (ADR-0036).
#include <astral/editor/world_authoring.hpp>
#include <astral/world/serialize.hpp>
#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace astral;
using namespace astral::editor;

namespace {

std::filesystem::path make_temp_root(const char* name) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir / "scenes", ec);
    std::filesystem::create_directories(dir / "maps", ec);
    return dir;
}

std::string read_text_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void write_text_file(const std::filesystem::path& p, std::string_view text) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

class SlopedTerrainSampler final : public IHeightSampler {
public:
    [[nodiscard]] double sample_height(double world_x, double world_z) const noexcept override {
        return 10.0 + 0.02 * world_x + 0.01 * world_z;
    }
};

} // namespace

TEST_CASE("WorldAuthoringSession creates and edits .world, .ent, data layers, and placement tools") {
    const std::filesystem::path root = make_temp_root("astral_e1_world_authoring_test");
    TypeRegistry types;
    register_world_types(types);
    LocalEditorFileSystem fs;

    WorldAuthoringSession wa(fs, types, root);
    MapDesc desc;
    desc.grids = {{.name = "main", .cell_size = 256.0, .load_range = 400.0}};
    desc.layers = {{.name = "day", .active = true}, {.name = "night", .active = false}};

    std::string err;
    const std::filesystem::path world_path = root / "maps" / "authoring_test.world";
    REQUIRE(wa.create_map(world_path, desc, &err));
    CHECK(wa.is_open());
    CHECK(std::filesystem::exists(world_path));
    CHECK(std::filesystem::is_directory(wa.entities_dir()));

    // Toggle data layer and verify persistence across reopen.
    MapDesc updated = wa.map_desc();
    updated.layers[1].active = true;
    REQUIRE(wa.save_map_desc(updated, &err));

    // Data-driven placement tools: default + JSON-parsed custom tool.
    const std::vector<PlacementToolDef> tools = default_placement_tools();
    REQUIRE(tools.size() == 5);
    const std::optional<Json> custom_json = Json::parse(R"({
        "tools": [
            { "id": "pine", "label": "Pine Tree", "category": "Foliage", "scale": [1.5, 2.5, 1.5], "spacing": 12.0, "y_offset": 0.25 }
        ]
    })");
    REQUIRE(custom_json.has_value());
    const std::vector<PlacementToolDef> parsed_tools = parse_placement_tools(*custom_json, &err);
    REQUIRE(parsed_tools.size() == 1);
    CHECK(parsed_tools[0].category == PlacementCategory::Foliage);

    // Place items snapped to non-flat IHeightSampler (M15 terrain hook).
    const SlopedTerrainSampler slope;
    const auto building_id = wa.place_with_tool(tools[1], 100.0, 200.0, slope, &err);
    REQUIRE(building_id.has_value());
    const auto pine_id = wa.place_with_tool(parsed_tools[0], 300.0, 100.0, slope, &err);
    REQUIRE(pine_id.has_value());
    const std::array<double, 2> road_pts[2] = {{0.0, 0.0}, {40.0, 0.0}};
    const std::vector<std::uint64_t> road_ids = wa.place_spline_with_tool(tools[0], road_pts, slope, &err);
    CHECK(road_ids.size() >= 4);

    // Verify height snapping on building and pine.
    WorldAuthoringSession reopened(fs, types, root);
    REQUIRE(reopened.open_map(world_path, &err));
    CHECK(reopened.map_desc().layers[1].active == true);

    bool found_building = false;
    bool found_pine = false;
    for (const MapEntity& me : reopened.raw_entities()) {
        if (me.id == *building_id) {
            found_building = true;
            // slope(100, 200) = 10 + 2 + 2 = 14.0, default Building y_offset = 0.0
            CHECK(me.position[1] == doctest::Approx(14.0));
        }
        if (me.id == *pine_id) {
            found_pine = true;
            // slope(300, 100) = 10 + 6 + 1 = 17.0, pine y_offset = 0.25
            CHECK(me.position[1] == doctest::Approx(17.25));
        }
    }
    CHECK(found_building);
    CHECK(found_pine);

    // Verify cook cell ownership visualization.
    std::vector<std::string> cell_errs;
    const std::vector<CellOwnershipInfo> cells = reopened.inspect_cells(&cell_errs);
    CHECK(cell_errs.empty());

    // Verify WorldAuthoringSession undo/redo removes and restores placed .ent files atomically.
    const std::size_t before_count = wa.raw_entities().size();
    const auto extra_id = wa.place_with_tool(tools[1], 50.0, 60.0, slope, &err);
    REQUIRE(extra_id.has_value());
    CHECK(wa.raw_entities().size() == before_count + 1);
    CHECK(wa.can_undo());
    REQUIRE(wa.undo(&err));
    CHECK(wa.raw_entities().size() == before_count);
    CHECK_FALSE(std::filesystem::exists(wa.entities_dir() / entity_file_name(*extra_id)));
    REQUIRE(wa.redo(&err));
    CHECK(wa.raw_entities().size() == before_count + 1);
    CHECK(std::filesystem::exists(wa.entities_dir() / entity_file_name(*extra_id)));
    CHECK(cells.size() >= 2); // (0,0) and (1,0) at x=300 with cell_size=256
}

TEST_CASE("WorldAuthoringSession distinguishes Instance Override vs Edit-in-Context with clean file diffs") {
    const std::filesystem::path root = make_temp_root("astral_e1_override_vs_context_test");
    const std::filesystem::path scene_path = root / "scenes" / "camp.scene";
    write_text_file(scene_path, R"({
        "format": "astral.scene",
        "version": 1,
        "entities": [
            {
                "id": 1,
                "components": {
                    "astral::Name": { "value": "Campfire" },
                    "astral::Transform": { "position": [0.0, 0.0, 0.0], "rotation": [0.0, 0.0, 0.0, 1.0], "scale": [1.0, 1.0, 1.0] }
                }
            },
            {
                "id": 2,
                "components": {
                    "astral::Name": { "value": "Tent" },
                    "astral::Transform": { "position": [4.0, 0.0, 0.0], "rotation": [0.0, 0.0, 0.0, 1.0], "scale": [1.0, 1.0, 1.0] }
                }
            }
        ]
    })");

    TypeRegistry types;
    register_world_types(types);
    LocalEditorFileSystem fs;
    WorldAuthoringSession wa(fs, types, root);

    MapDesc desc;
    desc.grids = {{.name = "main", .cell_size = 256.0, .load_range = 400.0}};
    std::string err;
    REQUIRE(wa.create_map(root / "maps" / "camp_map.world", desc, &err));

    LevelInstancePlacement inst_a{.instance_id = 1001, .scene_path = "scenes/camp.scene", .position = {50.0, 0.0, 50.0}};
    LevelInstancePlacement inst_b{.instance_id = 1002, .scene_path = "scenes/camp.scene", .position = {600.0, 0.0, 600.0}};
    REQUIRE(wa.place_level_instance(inst_a, &err));
    REQUIRE(wa.place_level_instance(inst_b, &err));

    const std::filesystem::path ent_a = wa.entities_dir() / "1001.ent";
    const std::filesystem::path ent_b = wa.entities_dir() / "1002.ent";
    const std::string scene_before = read_text_file(scene_path);
    const std::string ent_b_before = read_text_file(ent_b);

    // 1. Instance Override: modifies ONLY 1001.ent; source scene & 1002.ent stay byte-identical.
    REQUIRE(wa.edit_instance_override(1001, "1", "astral::Name", "value", Json("North Campfire"), &err));
    CHECK(read_text_file(scene_path) == scene_before);
    CHECK(read_text_file(ent_b) == ent_b_before);

    // 2. Edit-in-Context: modifies source camp.scene directly; both instances receive updated Tent name,
    //    while 1001 retains its North Campfire override on hot-reload expansion!
    const std::string ent_a_after_override = read_text_file(ent_a);
    REQUIRE(wa.edit_source_scene_in_context("scenes/camp.scene", 2, "astral::Name", "value", Json("Winter Tent"), &err));
    CHECK(read_text_file(scene_path) != scene_before);
    CHECK(read_text_file(ent_a) == ent_a_after_override);
    CHECK(read_text_file(ent_b) == ent_b_before);

    std::vector<std::string> expand_errs;
    const MapSource expanded = wa.build_expanded_source(&expand_errs);
    REQUIRE(expand_errs.empty());
    REQUIRE(expanded.entities.size() == 4);

    std::string a_fire, a_tent, b_fire, b_tent;
    for (const MapEntity& me : expanded.entities) {
        const auto inst = static_cast<std::uint64_t>(me.record["components"]["astral::LevelInstanceMember"]["instance_id"].as_number());
        const std::string local = me.record["components"]["astral::LevelInstanceMember"]["local_path"].as_string();
        const std::string name = me.record["components"]["astral::Name"]["value"].as_string();
        if (inst == 1001 && local == "1") a_fire = name;
        if (inst == 1001 && local == "2") a_tent = name;
        if (inst == 1002 && local == "1") b_fire = name;
        if (inst == 1002 && local == "2") b_tent = name;
    }
    CHECK(a_fire == "North Campfire");
    CHECK(a_tent == "Winter Tent");
    CHECK(b_fire == "Campfire");
    CHECK(b_tent == "Winter Tent");
}
