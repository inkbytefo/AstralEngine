#pragma once
// Live map content (ADR-0021): spawning and removing a cell's entities, data layers, and the changes
// the game made to a cell (destroyed or modified entities), remembered while the cell is unloaded.

#include <astral/world/map.hpp>
#include <astral/world/origin.hpp>
#include <astral/world/scene.hpp>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace astral {

/// Component on every top-level entity spawned from a map cell (prefab parts follow their instance).
struct CellMember {
    CellKey cell;
    std::uint64_t id = 0; ///< MapEntity::id
};

/// Which data layers are on. Starts from the map's initial states.
class MapLayers {
public:
    explicit MapLayers(const MapDesc& map);
    [[nodiscard]] bool active(std::string_view layer) const noexcept;
    /// False if the map has no such layer.
    bool set(std::string_view layer, bool active);
    /// No layers → always visible; otherwise visible while any of its layers is on.
    [[nodiscard]] bool visible(const MapEntity& entity) const noexcept;
    [[nodiscard]] const std::vector<LayerDesc>& layers() const noexcept { return layers_; }
    /// Bumped by every change (callers refresh when it moves).
    [[nodiscard]] std::uint64_t version() const noexcept { return version_; }

private:
    std::vector<LayerDesc> layers_;
    std::uint64_t version_ = 0;
};

/// What the game changed in a cell: map entities it destroyed and the records of those it modified.
struct CellDiff {
    std::vector<std::uint64_t> removed;
    std::vector<Json> changed; ///< Full scene records ("id" = MapEntity::id).
};

/// Lifecycle status of a map entity id in MapState / MapRuntime (ADR-0034).
enum class MapEntityStatus : std::uint8_t { Unknown, Unloaded, Alive, Destroyed };

/// Persistent map-level entity reference paired with its owning cell.
struct MapEntityRef {
    std::uint64_t id = 0;
    CellKey cell{};
    [[nodiscard]] bool valid() const noexcept { return id != 0; }
    friend bool operator==(const MapEntityRef&, const MapEntityRef&) = default;
};

/// Result of resolving a MapEntity::id against live MapState.
struct MapEntityLookup {
    MapEntityStatus status = MapEntityStatus::Unknown;
    Entity entity{};
    CellKey cell{};
    std::uint32_t load_count = 0; ///< > 1 when the entity was unloaded and reloaded.
};

class MapState {
public:
    /// Spawns the cell's visible entities (with the remembered diff applied). Remembers the cell's
    /// entities as loaded until despawn_cell.
    LoadResult spawn_cell(World& world, PrefabLibrary& prefabs, CellKey cell, std::span<const MapEntity> entities,
                          const MapLayers& layers);
    /// Records what changed since spawn_cell, then destroys the cell's entities (and prefab parts).
    void despawn_cell(World& world, PrefabLibrary& prefabs, CellKey cell);
    /// Spawns/destroys layer entities in every loaded cell to match `layers`.
    void refresh_layers(World& world, PrefabLibrary& prefabs, const MapLayers& layers);

    /// Registers a map entity id known to belong to `cell` even before that cell streams in.
    void register_known(std::uint64_t id, CellKey cell);
    /// Resolves a persistent MapEntity::id into its lifecycle state and live Entity (if Alive).
    [[nodiscard]] MapEntityLookup lookup(World& world, std::uint64_t id) const;

    /// World position of local zero (floating origin). Spawned top-level entities are placed relative
    /// to it; remembered changes are kept in world coordinates.
    void set_origin(const WorldPosition& origin) noexcept { origin_ = origin; }
    [[nodiscard]] const WorldPosition& origin() const noexcept { return origin_; }

    /// Remembered changes of a cell; nullptr if none.
    [[nodiscard]] const CellDiff* diff(CellKey cell) const noexcept;

private:
    struct Loaded {
        CellKey key;
        std::vector<MapEntity> entities; ///< With the diff applied (removed ones dropped).
        std::vector<Json> baseline;      ///< Saved records right after spawning, by MapEntity order.
        std::vector<bool> spawned;
    };
    struct Remembered {
        CellKey key;
        CellDiff diff;
    };
    struct KnownRecord {
        std::uint64_t id = 0;
        CellKey cell{};
        std::uint32_t load_count = 0;
    };

    Loaded* find_loaded(CellKey cell) noexcept;
    LoadResult spawn(World& world, PrefabLibrary& prefabs, Loaded& cell, std::size_t index);
    /// Merges what the game changed in a cell into its remembered diff (later records replace earlier ones).
    void remember(CellKey cell, CellDiff fresh);
    KnownRecord& ensure_known(std::uint64_t id, CellKey cell);

    Json save_world_record(World& world, PrefabLibrary& prefabs, Entity e) const;

    WorldPosition origin_{};
    std::vector<Loaded> loaded_;
    std::vector<Remembered> remembered_;
    std::vector<KnownRecord> known_;
};

} // namespace astral

