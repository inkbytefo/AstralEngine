#pragma once
// Maps (ADR-0021): a `.world` document (settings, streaming grids, data layers) plus one `.ent` file per
// placed entity. Pure data and functions: no file system, no ECS. Positions are double (large worlds).

#include <astral/core/json.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace astral {

/// A streaming grid: entities on it are loaded when a streaming source is within `load_range`.
struct GridDesc {
    std::string name = "main";
    double cell_size = 128.0;  ///< Metres; cells are squares on the ground (x, z) plane.
    double load_range = 384.0; ///< Metres from a streaming source to the cell's square.
};

/// A data layer: a tag on entities that can be switched on and off while the game runs.
struct LayerDesc {
    std::string name;
    bool active = true; ///< Initial state.
};

/// The `.world` document ("astral.world" v1). At least one grid; the first is the default.
struct MapDesc {
    Json settings = Json::object(); ///< Game/world settings (free-form, read by the game).
    std::vector<GridDesc> grids;
    std::vector<LayerDesc> layers;
};

/// Reads a `.world` document; nullopt (with `error`) on a wrong format/version. No grids → "main".
std::optional<MapDesc> parse_map(const Json& document, std::string* error = nullptr);
[[nodiscard]] Json write_map(const MapDesc& map);

/// One placed entity ("astral.entity" v1 file): a scene entity record plus where it streams.
struct MapEntity {
    std::uint64_t id = 0;
    Json record;                            ///< Same shape as an entity in an "astral.scene" document.
    std::string grid;                       ///< Grid name; empty → the map's first grid.
    std::vector<std::string> layers;        ///< Data layers; empty → always part of the world.
    bool always_loaded = false;             ///< Not streamed (game rules, sky, player).
    std::array<double, 3> position{0, 0, 0}; ///< From the record's Transform (for cell assignment).
};

/// An entity file as read from disk / a pack (the caller lists and parses the files).
struct EntityFile {
    std::string path;
    Json document;
};

struct MapSource {
    MapDesc map;
    std::vector<MapEntity> entities; ///< Sorted by id.
    std::vector<std::string> errors; ///< "path: problem" for skipped or corrected files.
};

/// Validates and collects entity files. Skips wrong formats, missing or duplicate ids (reported);
/// unknown grids fall back to the first grid (reported).
[[nodiscard]] MapSource read_map_source(const MapDesc& map, std::span<const EntityFile> files);
/// The `.ent` document for an entity (record id is set to `entity.id`).
[[nodiscard]] Json make_entity_file(const MapEntity& entity);
/// Stable file name for an entity id: "<id>.ent".
[[nodiscard]] std::string entity_file_name(std::uint64_t id);

/// A cell: grid index into MapDesc::grids and integer coordinates on the ground plane.
struct CellKey {
    std::uint32_t grid = 0;
    std::int32_t x = 0;
    std::int32_t z = 0;
    friend bool operator==(const CellKey&, const CellKey&) = default;
};

struct Cell {
    CellKey key;
    std::vector<std::size_t> entities; ///< Indices into the entity list given to assign_cells.
};

struct CellAssignment {
    std::vector<Cell> cells;                ///< Sorted by grid, z, x; no empty cells.
    std::vector<std::size_t> always_loaded; ///< Entities that are never streamed.
};

/// Puts each entity into the cell containing its position (floor(position / cell_size)).
[[nodiscard]] CellAssignment assign_cells(const MapDesc& map, std::span<const MapEntity> entities);
/// "<grid>_<x>_<z>", e.g. "main_-1_2" (cook output file stem).
[[nodiscard]] std::string cell_name(const MapDesc& map, CellKey key);
[[nodiscard]] double cell_min_x(const MapDesc& map, CellKey key);
[[nodiscard]] double cell_min_z(const MapDesc& map, CellKey key);
/// Ground-plane distance from (x, z) to the cell's square; 0 inside.
[[nodiscard]] double cell_distance(const MapDesc& map, CellKey key, double x, double z);

/// Cook output of a map: an index (the map document plus "cells": [{name, grid, x, z, entities}] and
/// "always_loaded": count) and one "astral.cell" document per cell ("always" first, then sorted cells).
struct CookedCell {
    std::string name;
    Json document;
};
struct CookedMap {
    Json index;
    std::vector<CookedCell> cells;
};
[[nodiscard]] CookedMap cook_map(const MapSource& source);
/// Entities of a cooked cell document (empty on a wrong format).
[[nodiscard]] std::vector<MapEntity> read_cell(const MapDesc& map, const Json& cell);

} // namespace astral
