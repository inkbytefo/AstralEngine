#pragma once
// Level Instances and Cross-Cell References (E1, ADR-0036):
// Places reusable `.scene` documents into open-world maps with placement transforms (never entity parenting),
// per-instance overrides, nested cycle/limit diagnostics, multi-cell ownership, and persistent cross-cell refs.

#include <astral/core/json.hpp>
#include <astral/core/math.hpp>
#include <astral/core/reflect.hpp>
#include <astral/world/map.hpp>
#include <astral/world/scene.hpp>
#include <astral/world/serialize.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace astral {

/// Component stamped on every entity expanded from a Level Instance (no artificial Parent chain).
struct LevelInstanceMember {
    std::uint64_t instance_id = 0; ///< Placing MapEntity::id in the map.
    std::string source_scene;      ///< Source `.scene` asset path.
    std::string local_path;        ///< Stable local record id / nested path inside source scene ("1", "2/3").
};

/// Persistent reference across streaming cells or Level Instances (resolved via MapState::lookup).
struct CrossCellRef {
    std::uint64_t target_id = 0; ///< Persistent MapEntity::id (or deterministic expanded member id).
    std::string target_label;    ///< Optional human-readable label for diagnostics/editor display.
};

/// Placement of a `.scene` asset inside a `.world` map (or nested inside another `.scene`).
struct LevelInstancePlacement {
    std::uint64_t instance_id = 0;            ///< Unique id in the owning map (1 .. 2^32-1).
    std::string scene_path;                   ///< Source `.scene` asset path.
    std::array<double, 3> position{0, 0, 0};  ///< World-space placement origin.
    Quat rotation{};                          ///< World-space placement rotation.
    Vec3 scale{1.0f, 1.0f, 1.0f};             ///< Placement scale applied to root positions/scales.
    std::string grid;                         ///< Optional grid name; empty -> default grid.
    std::vector<std::string> layers;          ///< Data layers inherited by expanded entities.
    bool always_loaded = false;
    Json overrides = Json::object();          ///< Map: local_path -> {"components": {...}, "removed": [...]}.
    std::vector<std::string> deleted;         ///< Local paths deleted in this instance.
};

struct LevelInstanceLimits {
    std::uint32_t max_depth = 8;
    std::size_t max_entities = 10000;
};

struct LevelInstanceExpandResult {
    std::vector<MapEntity> entities;
    std::vector<std::string> errors;
    std::vector<std::string> sources; ///< Every source `.scene` document read during expansion.
    bool truncated = false;           ///< True if `max_entities` was exceeded.
};

/// Deterministically derives a non-colliding MapEntity::id (1 .. 2^31-1) for an inner entity of a Level Instance.
[[nodiscard]] std::uint64_t level_instance_entity_id(std::uint64_t instance_id, std::string_view local_path) noexcept;

/// Serializes / parses a LevelInstancePlacement to/from a MapEntity `.ent` document ("astral.entity" v1).
[[nodiscard]] Json make_level_instance_entity_file(const LevelInstancePlacement& placement);
[[nodiscard]] std::optional<LevelInstancePlacement> parse_level_instance_entity(const Json& entity_doc);

/// Expands one LevelInstancePlacement into world-space MapEntities (without entity parenting).
/// Validates nested cycles, max depth, and max entity limits; merges per-instance overrides and deletions.
[[nodiscard]] LevelInstanceExpandResult expand_level_instance(const LevelInstancePlacement& placement,
                                                              const TypeRegistry& types,
                                                              const DocumentSource& docs,
                                                              const LevelInstanceLimits& limits = {});

/// Expands every Level Instance inside `source` into world-space MapEntities ready for assign_cells() / cook_map().
/// Plain MapEntities are preserved unchanged; expanded inner entities receive their own world-space cell assignment.
[[nodiscard]] MapSource expand_map_level_instances(const MapSource& source, const TypeRegistry& types,
                                                   const DocumentSource& docs,
                                                   const LevelInstanceLimits& limits = {});

/// Computes/merges a single field override onto `placement.overrides` for `local_path` without touching the source scene.
void set_level_instance_override(LevelInstancePlacement& placement, std::string_view local_path,
                                 std::string_view component, std::string_view field, const Json& value);

/// Registers E1 Level Instance & CrossCellRef components in `types`.
void register_level_instance_types(TypeRegistry& types);

} // namespace astral

ASTRAL_REFLECT(astral::LevelInstanceMember, instance_id, source_scene, local_path)
ASTRAL_REFLECT(astral::CrossCellRef, target_id, target_label)
