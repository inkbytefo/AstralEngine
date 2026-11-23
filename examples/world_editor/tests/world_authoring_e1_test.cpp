// End-to-end E1 acceptance test (ADR-0036):
// 2x2 km editable `.world` map with two independent Level Instances, data-driven placement tools
// snapped to a non-flat terrain IHeightSampler, Edit-in-Context vs Instance Override with hot-reload
// override merge, cook consistency between WorldAuthoringSession and astral-cook (cook_directory),
// and playable App streaming + floating origin shift preserving LevelInstanceMember IDs and
// CrossCellRef across cell unload/reload.
#include <astral/assets/cook.hpp>
#include <astral/editor/workspace.hpp>
#include <astral/world/level_instance.hpp>
#include <astral/world/streaming.hpp>
#include <astral/world/transform.hpp>
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace astral;
using namespace astral::editor;

namespace {

std::filesystem::path make_e1_sandbox() {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "astral_e1_2x2km_acceptance";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / "scenes", ec);
    std::filesystem::create_directories(root / "maps", ec);
    return root;
}

std::string read_file_str(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void write_file_str(const std::filesystem::path& p, std::string_view text) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

class AnalyticTerrainSampler final : public IHeightSampler {
public:
    [[nodiscard]] double sample_height(double world_x, double world_z) const noexcept override {
        return 5.0 + 0.01 * world_x + 0.005 * world_z;
    }
};

} // namespace

TEST_CASE("E1 2x2 km editable map: two Level Instances, placement, override/edit-in-context, cook parity, streaming unload/reload") {
    const std::filesystem::path root = make_e1_sandbox();
    const char* stub_prefab = R"({
        "format": "astral.scene",
        "version": 1,
        "entities": [{ "id": 1, "components": { "astral::Name": { "value": "Prop" }, "astral::Transform": {} } }]
    })";
    write_file_str(root / "prefabs" / "house.prefab", stub_prefab);
    write_file_str(root / "prefabs" / "pine.prefab", stub_prefab);
    write_file_str(root / "prefabs" / "rock.prefab", stub_prefab);
    const std::filesystem::path outpost_scene = root / "scenes" / "outpost.scene";
    write_file_str(outpost_scene, R"({
        "format": "astral.scene",
        "version": 1,
        "entities": [
            {
                "id": 1,
                "components": {
                    "astral::Name": { "value": "Watchtower" },
                    "astral::Transform": { "position": [0.0, 0.0, 0.0], "rotation": [0.0, 0.0, 0.0, 1.0], "scale": [1.0, 1.0, 1.0] }
                }
            },
            {
                "id": 2,
                "components": {
                    "astral::Name": { "value": "SupplyCrate" },
                    "astral::Transform": { "position": [12.0, 0.0, -4.0], "rotation": [0.0, 0.0, 0.0, 1.0], "scale": [1.0, 1.0, 1.0] }
                }
            }
        ]
    })");

    const std::filesystem::path world_file = root / "maps" / "frontier_2km.world";
    const std::uint64_t inst_a_id = 1001;
    const std::uint64_t inst_b_id = 1002;
    const std::uint64_t anchor_id = 2001;
    const std::uint64_t inst_a_tower_id = level_instance_entity_id(inst_a_id, "1");
    const std::uint64_t inst_b_tower_id = level_instance_entity_id(inst_b_id, "1");
    REQUIRE(inst_a_tower_id != inst_b_tower_id);

    // Phase 1: Author the 2x2 km map via EditorWorkspace + WorldAuthoringSession.
    {
        struct AuthorDriver final : Game {
            EditorWorkspace editor;
            std::filesystem::path world_path;
            std::filesystem::path scene_path;
            std::uint64_t inst_a;
            std::uint64_t inst_b;
            std::uint64_t anchor;
            std::uint64_t target_b_tower;
            bool ok = false;

            AuthorDriver(std::filesystem::path r, std::filesystem::path w, std::filesystem::path s,
                         std::uint64_t a, std::uint64_t b, std::uint64_t anc, std::uint64_t tb)
                : editor({.asset_root = std::move(r)}),
                  world_path(std::move(w)),
                  scene_path(std::move(s)),
                  inst_a(a),
                  inst_b(b),
                  anchor(anc),
                  target_b_tower(tb) {}

            void on_start(Context& ctx) override {
                editor.on_start(ctx);
                WorldAuthoringSession* wa = editor.world_authoring();
                REQUIRE(wa != nullptr);

                MapDesc desc;
                desc.grids = {{.name = "main", .cell_size = 256.0, .load_range = 320.0}};
                desc.layers = {{.name = "base", .active = true}, {.name = "props", .active = true}};
                std::string err;
                REQUIRE(wa->create_map(world_path, desc, &err));

                // Place two independent Level Instances ~2.2 km apart across the 2x2 km map.
                LevelInstancePlacement p_a{.instance_id = inst_a, .scene_path = "scenes/outpost.scene", .position = {120.0, 0.0, 120.0}};
                LevelInstancePlacement p_b{.instance_id = inst_b, .scene_path = "scenes/outpost.scene", .position = {1750.0, 0.0, 1750.0}};
                REQUIRE(wa->place_level_instance(p_a, &err));
                REQUIRE(wa->place_level_instance(p_b, &err));

                // Place a cross-cell anchor near Instance A referencing Instance B's Watchtower.
                MapEntity anchor_ent;
                anchor_ent.id = anchor;
                anchor_ent.position = {125.0, 0.0, 125.0};
                anchor_ent.record = Json::object();
                anchor_ent.record["id"] = static_cast<double>(anchor);
                Json comps = Json::object();
                Json nm = Json::object();
                nm["value"] = "DispatchAnchor";
                comps["astral::Name"] = std::move(nm);
                Json tf = Json::object();
                tf["position"] = Json::array();
                tf["position"].push_back(125.0);
                tf["position"].push_back(0.0);
                tf["position"].push_back(125.0);
                comps["astral::Transform"] = std::move(tf);
                Json ref = Json::object();
                ref["target_id"] = static_cast<double>(target_b_tower);
                ref["role"] = "remote_watchtower";
                comps["astral::CrossCellRef"] = std::move(ref);
                anchor_ent.record["components"] = std::move(comps);
                REQUIRE(wa->upsert_entity(anchor_ent, &err));

                // Data-driven placement (Road spline + Building + Foliage + Rock + Prefab) using M15-ready height hook.
                const AnalyticTerrainSampler terrain;
                const auto& tools = editor.placement_tools();
                REQUIRE(tools.size() >= 5);
                const std::array<double, 2> road_pts[2] = {{100.0, 100.0}, {160.0, 100.0}};
                CHECK(wa->place_spline_with_tool(tools[0], road_pts, terrain, &err).size() >= 4);
                CHECK(wa->place_with_tool(tools[1], 140.0, 140.0, terrain, &err).has_value());
                CHECK(wa->place_with_tool(tools[2], 900.0, 900.0, terrain, &err).has_value());
                CHECK(wa->place_with_tool(tools[3], 1720.0, 1720.0, terrain, &err).has_value());

                // Separate WorldView preview (never opens playable ctx.maps()).
                REQUIRE(editor.open_world_map(ctx, "maps/frontier_2km.world", &err));
                CHECK(ctx.maps().world_streaming() == nullptr);
                CHECK(wa->inspect_cells().size() >= 3);

                // Instance Override on Instance A only -> Instance B .ent & source scene stay byte-identical.
                const std::filesystem::path ent_b_path = wa->entities_dir() / entity_file_name(inst_b);
                const std::string ent_b_before = read_file_str(ent_b_path);
                const std::string scene_before = read_file_str(scene_path);
                REQUIRE(wa->edit_instance_override(inst_a, "1", "astral::Name", "value", Json("South Watchtower"), &err));
                CHECK(read_file_str(ent_b_path) == ent_b_before);
                CHECK(read_file_str(scene_path) == scene_before);

                // Edit-in-Context on outpost.scene -> affects both instances while keeping Instance A's override.
                REQUIRE(wa->edit_source_scene_in_context("scenes/outpost.scene", 2, "astral::Name", "value", Json("AmmoCrate"), &err));
                CHECK(read_file_str(scene_path) != scene_before);
                REQUIRE(editor.reload_world_preview(ctx, &err));
                CHECK(find_by_name(ctx.world, "South Watchtower").valid());
                CHECK(find_by_name(ctx.world, "Watchtower").valid()); // Instance B untouched
                CHECK(find_by_name(ctx.world, "AmmoCrate").valid());  // Propagated from source scene

                // Cook consistency check: editor cook_current_map() vs astral-cook cook_directory().
                std::vector<std::string> cook_errs;
                const CookedMap editor_cooked = wa->cook_current_map(&cook_errs);
                CHECK(cook_errs.empty());
                const std::filesystem::path cooked_dir = std::filesystem::temp_directory_path() / "astral_e1_2x2km_cooked";
                std::error_code ec;
                std::filesystem::remove_all(cooked_dir, ec);
                const CookReport report = cook_directory({.source = world_path.parent_path().parent_path(), .output = cooked_dir});
                CHECK(report.ok());
                const std::filesystem::path cooked_cells_dir = cooked_dir / "maps" / "frontier_2km.cells";
                REQUIRE(std::filesystem::is_directory(cooked_cells_dir));
                for (const CookedCell& cell : editor_cooked.cells) {
                    const std::filesystem::path cell_file = cooked_cells_dir / (cell.name + ".cell");
                    CHECK(std::filesystem::exists(cell_file));
                }

                ok = true;
            }
            void on_update(Context& ctx) override { editor.on_update(ctx); }
            void on_shutdown(Context& ctx) override { editor.on_shutdown(ctx); }
        };

        App editor_app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = root.string(), .hot_reload = false});
        AuthorDriver driver(root, world_file, outpost_scene, inst_a_id, inst_b_id, anchor_id, inst_b_tower_id);
        editor_app.run_frames(driver, 2);
        REQUIRE(driver.ok);
    }

    // Phase 2: Playable App opens the same 2x2 km map, streams cells + floating origin across 2 km,
    // and verifies LevelInstanceMember identity + CrossCellRef preservation across unload/reload.
    {
        struct PlayableDriver final : Game {
            std::uint64_t a_tower;
            std::uint64_t b_tower;
            std::uint64_t anchor;
            Entity player{};
            bool ok = false;

            PlayableDriver(std::uint64_t at, std::uint64_t bt, std::uint64_t anc)
                : a_tower(at), b_tower(bt), anchor(anc) {}

            void on_start(Context& ctx) override {
                player = ctx.world.create(
                    Name{"Player"},
                    Transform{.position = {120.0f, 0.0f, 120.0f}},
                    StreamingSource{});
                REQUIRE(ctx.maps().open_world("maps/frontier_2km.world"));
                ctx.maps().finish_world_loading();

                // At (120, 0, 120): Instance A + DispatchAnchor are loaded; Instance B (1750, 0, 1750) is unloaded.
                const MapEntityLookup a_lookup = ctx.maps().resolve_map_entity(a_tower);
                REQUIRE(a_lookup.status == MapEntityStatus::Alive);
                REQUIRE(ctx.world.has<LevelInstanceMember>(a_lookup.entity));
                CHECK(ctx.world.get<LevelInstanceMember>(a_lookup.entity)->instance_id == 1001);
                CHECK(ctx.world.get<Name>(a_lookup.entity)->value == "South Watchtower");
                CHECK_FALSE(ctx.world.has<Parent>(a_lookup.entity)); // Placement without artificial parenting

                const MapEntityLookup anc_lookup = ctx.maps().resolve_map_entity(anchor);
                REQUIRE(anc_lookup.status == MapEntityStatus::Alive);
                REQUIRE(ctx.world.has<CrossCellRef>(anc_lookup.entity));
                CHECK(ctx.world.get<CrossCellRef>(anc_lookup.entity)->target_id == b_tower);
                CHECK(ctx.maps().resolve_map_entity(b_tower).status == MapEntityStatus::Unloaded);

                // Travel ~2.3 km to Instance B -> triggers cell unload of (0,0), load of (6,6), and floating origin shift.
                auto* ptf = ctx.world.get<Transform>(player);
                REQUIRE(ptf != nullptr);
                const WorldPosition origin_before = ctx.maps().world_origin();
                ptf->position = {static_cast<float>(1750.0 - origin_before.x), 0.0f, static_cast<float>(1750.0 - origin_before.z)};
                ctx.maps().finish_world_loading();

                CHECK(ctx.maps().resolve_map_entity(a_tower).status == MapEntityStatus::Unloaded);
                const MapEntityLookup b_lookup = ctx.maps().resolve_map_entity(b_tower);
                REQUIRE(b_lookup.status == MapEntityStatus::Alive);
                REQUIRE(ctx.world.has<LevelInstanceMember>(b_lookup.entity));
                CHECK(ctx.world.get<LevelInstanceMember>(b_lookup.entity)->instance_id == 1002);
                CHECK(ctx.world.get<Name>(b_lookup.entity)->value == "Watchtower");

                // Return to Instance A -> reloads (0,0) with identical LevelInstanceMember IDs and CrossCellRef!
                const WorldPosition origin_far = ctx.maps().world_origin();
                ptf->position = {static_cast<float>(120.0 - origin_far.x), 0.0f, static_cast<float>(120.0 - origin_far.z)};
                ctx.maps().finish_world_loading();

                const MapEntityLookup a_reloaded = ctx.maps().resolve_map_entity(a_tower);
                REQUIRE(a_reloaded.status == MapEntityStatus::Alive);
                CHECK(ctx.world.get<LevelInstanceMember>(a_reloaded.entity)->instance_id == 1001);
                CHECK(ctx.world.get<Name>(a_reloaded.entity)->value == "South Watchtower");

                const MapEntityLookup anc_reloaded = ctx.maps().resolve_map_entity(anchor);
                REQUIRE(anc_reloaded.status == MapEntityStatus::Alive);
                REQUIRE(ctx.world.has<CrossCellRef>(anc_reloaded.entity));
                CHECK(ctx.world.get<CrossCellRef>(anc_reloaded.entity)->target_id == b_tower);
                CHECK(ctx.maps().resolve_map_entity(b_tower).status == MapEntityStatus::Unloaded);

                ok = true;
            }
            void on_update(Context&) override {}
        };

        App play_app({.headless = true, .fixed_timestep = 1.0f / 60.0f, .asset_root = root.string(), .hot_reload = false});
        PlayableDriver driver(inst_a_tower_id, inst_b_tower_id, anchor_id);
        play_app.run_frames(driver, 2);
        REQUIRE(driver.ok);
    }
}
