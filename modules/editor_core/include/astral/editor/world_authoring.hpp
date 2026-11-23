#pragma once
// E1 World Authoring & Data-Driven Placement Tools (ADR-0036):
// Edits `.world` and per-entity `<map>.entities/<id>.ent` files, manages Level Instance edit-in-context
// vs per-instance overrides, previews cook cells / layers / streaming ownership / floating origin,
// and provides data-driven placement tools (Road, Building, Foliage, Rock, Prefab, LevelInstance)
// with an M15-ready height-sampling hook (IHeightSampler).

#include <astral/editor/document.hpp>
#include <astral/editor/transaction.hpp>
#include <astral/world/level_instance.hpp>
#include <astral/world/map.hpp>
#include <astral/world/origin.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace astral::editor {

/// Terrain height / collision sampling hook for placement tools (integrates with M15 heightfields).
class IHeightSampler {
public:
    virtual ~IHeightSampler() = default;
    [[nodiscard]] virtual double sample_height(double world_x, double world_z) const noexcept = 0;
};

/// Default flat-ground height sampler (used when no M15 terrain asset is attached).
class FlatHeightSampler final : public IHeightSampler {
public:
    explicit FlatHeightSampler(double height = 0.0) noexcept : height_(height) {}
    [[nodiscard]] double sample_height(double, double) const noexcept override { return height_; }

private:
    double height_ = 0.0;
};

enum class PlacementCategory : std::uint8_t { Road, Building, Foliage, Rock, Prefab, LevelInstance };

[[nodiscard]] std::string_view placement_category_name(PlacementCategory cat) noexcept;

/// Data-driven definition for a world placement tool (loaded from JSON or default palette).
struct PlacementToolDef {
    std::string id;
    std::string label;
    PlacementCategory category = PlacementCategory::Prefab;
    std::string asset_path;        ///< `.prefab` or `.scene` path (or empty for primitive placeholder).
    std::string default_grid;      ///< Target streaming grid; empty -> map default.
    std::vector<std::string> layers;
    Vec3 default_scale{1.0f, 1.0f, 1.0f};
    double spacing = 8.0;          ///< Spline/scatter spacing in metres (Road/Foliage/Rock).
    double y_offset = 0.0;         ///< Vertical offset above sampled terrain height.
};

/// Returns the built-in data-driven placement tool palette (Road, Building, Foliage, Rock, Prefab).
[[nodiscard]] std::vector<PlacementToolDef> default_placement_tools();
/// Parses a JSON array of PlacementToolDef entries.
[[nodiscard]] std::vector<PlacementToolDef> parse_placement_tools(const Json& doc, std::string* error = nullptr);

/// Summary of one cooked cell for editor visualization (streaming ownership + entity count).
struct CellOwnershipInfo {
    CellKey key{};
    std::string name;
    double min_x = 0.0;
    double min_z = 0.0;
    double size = 0.0;
    std::size_t entity_count = 0;
};

/// Authoring view of an open `.world` map and its `<stem>.entities/<id>.ent` files.
class WorldAuthoringSession {
public:
    WorldAuthoringSession(IEditorFileSystem& fs, const TypeRegistry& types, std::filesystem::path asset_root = {});

    bool create_map(const std::filesystem::path& world_path, const MapDesc& desc, std::string* error = nullptr);
    bool open_map(const std::filesystem::path& world_path, std::string* error = nullptr);
    [[nodiscard]] bool is_open() const noexcept { return open_; }

    [[nodiscard]] const std::filesystem::path& world_path() const noexcept { return world_path_; }
    [[nodiscard]] const std::filesystem::path& entities_dir() const noexcept { return entities_dir_; }
    [[nodiscard]] const MapDesc& map_desc() const noexcept { return map_; }
    [[nodiscard]] const std::vector<MapEntity>& raw_entities() const noexcept { return raw_entities_; }
    [[nodiscard]] const WorldPosition& preview_origin() const noexcept { return preview_origin_; }
    void set_preview_origin(WorldPosition origin) noexcept { preview_origin_ = origin; }

    /// Allocates the next unused positive MapEntity::id in the open map.
    [[nodiscard]] std::uint64_t next_entity_id() const noexcept;

    /// Persists a `.world` descriptor update (grids, layers, settings) atomically.
    bool save_map_desc(const MapDesc& updated, std::string* error = nullptr);
    /// Writes or updates one `<id>.ent` source file atomically.
    bool upsert_entity(const MapEntity& entity, std::string* error = nullptr);
    /// Places a Level Instance `.ent` file atomically.
    bool place_level_instance(const LevelInstancePlacement& placement, std::string* error = nullptr);

    /// Instance Override: edits one field of an inner entity ONLY on `instance_id`'s `.ent` file.
    bool edit_instance_override(std::uint64_t instance_id, std::string_view local_path, std::string_view component,
                                std::string_view field, const Json& value, std::string* error = nullptr);

    /// Edit-in-Context: edits the source `.scene` document directly via atomic save (affects all instances on reload,
    /// while preserving each instance's own overrides).
    bool edit_source_scene_in_context(std::string_view scene_path, std::uint32_t record_id, std::string_view component,
                                      std::string_view field, const Json& value, std::string* error = nullptr);

    /// Data-driven placement: places one item snapped to `height_sampler` and saves its `.ent` file.
    std::optional<std::uint64_t> place_with_tool(const PlacementToolDef& tool, double world_x, double world_z,
                                                 const IHeightSampler& height_sampler, std::string* error = nullptr);

    /// Places a series of road / spline segments between control points snapped to terrain height.
    std::vector<std::uint64_t> place_spline_with_tool(const PlacementToolDef& tool,
                                                      std::span<const std::array<double, 2>> points,
                                                      const IHeightSampler& height_sampler, std::string* error = nullptr);

    /// Expands all Level Instances (re-reading source `.scene` files for hot reload) and computes cook cells.
    [[nodiscard]] MapSource build_expanded_source(std::vector<std::string>* errors = nullptr) const;
    [[nodiscard]] CookedMap cook_current_map(std::vector<std::string>* errors = nullptr) const;
    [[nodiscard]] std::vector<CellOwnershipInfo> inspect_cells(std::vector<std::string>* errors = nullptr) const;

    [[nodiscard]] AuditLog& audit_log() noexcept { return audit_; }
    [[nodiscard]] const AuditLog& audit_log() const noexcept { return audit_; }

private:
    std::filesystem::path resolve_asset(std::string_view rel) const;
    bool write_entity_file(std::uint64_t id, const Json& doc, std::string* error);

    IEditorFileSystem& fs_;
    const TypeRegistry& types_;
    std::filesystem::path asset_root_;
    std::filesystem::path world_path_;
    std::filesystem::path entities_dir_;
    MapDesc map_;
    std::vector<MapEntity> raw_entities_;
    WorldPosition preview_origin_{};
    AuditLog audit_;
    bool open_ = false;
};

} // namespace astral::editor
