#include <astral/world/map.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace astral {

namespace {

constexpr const char* kMapFormat = "astral.world";
constexpr const char* kEntityFormat = "astral.entity";
constexpr double kVersion = 1.0;

bool check_format(const Json& doc, const char* format, std::string* error) {
    if (doc["format"].as_string() != format) {
        if (error) *error = std::string("not an ") + format + " document";
        return false;
    }
    if (doc["version"].as_number() != kVersion) {
        if (error) *error = std::string("unsupported ") + format + " version";
        return false;
    }
    return true;
}

std::int32_t cell_coord(double value, double size) {
    return static_cast<std::int32_t>(std::floor(value / size));
}

std::size_t grid_index(const MapDesc& map, std::string_view name, bool* found) {
    for (std::size_t i = 0; i < map.grids.size(); ++i) {
        if (map.grids[i].name == name) {
            *found = true;
            return i;
        }
    }
    *found = name.empty();
    return 0;
}

const GridDesc& grid_of(const MapDesc& map, CellKey key) {
    static const GridDesc fallback;
    return key.grid < map.grids.size() ? map.grids[key.grid] : fallback;
}

} // namespace

std::optional<MapDesc> parse_map(const Json& document, std::string* error) {
    if (!check_format(document, kMapFormat, error)) return std::nullopt;
    MapDesc map;
    if (document["settings"].is_object()) map.settings = document["settings"];
    for (const Json& g : document["grids"].as_array()) {
        GridDesc grid;
        if (g["name"].is_string()) grid.name = g["name"].as_string();
        grid.cell_size = g["cell_size"].as_number(grid.cell_size);
        grid.load_range = g["load_range"].as_number(grid.load_range);
        if (grid.cell_size <= 0.0) {
            if (error) *error = "grid '" + grid.name + "': cell_size must be positive";
            return std::nullopt;
        }
        map.grids.push_back(std::move(grid));
    }
    if (map.grids.empty()) map.grids.emplace_back();
    for (const Json& l : document["layers"].as_array()) {
        map.layers.push_back({l["name"].as_string(), l["active"].as_bool(true)});
    }
    return map;
}

Json write_map(const MapDesc& map) {
    Json doc = Json::object();
    doc["format"] = kMapFormat;
    doc["version"] = kVersion;
    doc["settings"] = map.settings;
    Json grids = Json::array();
    for (const GridDesc& g : map.grids) {
        Json j = Json::object();
        j["name"] = g.name;
        j["cell_size"] = g.cell_size;
        j["load_range"] = g.load_range;
        grids.push_back(std::move(j));
    }
    doc["grids"] = std::move(grids);
    Json layers = Json::array();
    for (const LayerDesc& l : map.layers) {
        Json j = Json::object();
        j["name"] = l.name;
        j["active"] = l.active;
        layers.push_back(std::move(j));
    }
    doc["layers"] = std::move(layers);
    return doc;
}

MapSource read_map_source(const MapDesc& map, std::span<const EntityFile> files) {
    MapSource out;
    out.map = map;
    std::map<std::uint64_t, std::string> seen; // id → first file
    for (const EntityFile& file : files) {
        std::string error;
        if (!check_format(file.document, kEntityFormat, &error)) {
            out.errors.push_back(file.path + ": " + error);
            continue;
        }
        const Json& record = file.document["entity"];
        const double id = record["id"].as_number(-1.0);
        if (!record.is_object() || id < 1.0 || id != std::floor(id)) {
            out.errors.push_back(file.path + ": entity has no valid id");
            continue;
        }
        MapEntity e;
        e.id = static_cast<std::uint64_t>(id);
        if (auto [it, inserted] = seen.emplace(e.id, file.path); !inserted) {
            out.errors.push_back(file.path + ": duplicate id " + std::to_string(e.id) + " (also in " + it->second + ")");
            continue;
        }
        e.record = record;
        bool found = false;
        const std::size_t g = grid_index(map, file.document["grid"].as_string(), &found);
        if (!found) out.errors.push_back(file.path + ": unknown grid '" + file.document["grid"].as_string() + "'");
        e.grid = map.grids.empty() ? std::string() : map.grids[g].name;
        for (const Json& l : file.document["layers"].as_array()) e.layers.push_back(l.as_string());
        e.always_loaded = file.document["always_loaded"].as_bool(false);
        const Json& p = record["components"]["astral::Transform"]["position"];
        for (std::size_t i = 0; i < 3; ++i) e.position[i] = p[i].as_number(0.0);
        out.entities.push_back(std::move(e));
    }
    std::sort(out.entities.begin(), out.entities.end(),
              [](const MapEntity& a, const MapEntity& b) { return a.id < b.id; });
    return out;
}

Json make_entity_file(const MapEntity& entity) {
    Json doc = Json::object();
    doc["format"] = kEntityFormat;
    doc["version"] = kVersion;
    Json record = entity.record.is_object() ? entity.record : Json::object();
    record["id"] = entity.id;
    doc["entity"] = std::move(record);
    if (!entity.grid.empty()) doc["grid"] = entity.grid;
    if (!entity.layers.empty()) {
        Json layers = Json::array();
        for (const std::string& l : entity.layers) layers.push_back(l);
        doc["layers"] = std::move(layers);
    }
    if (entity.always_loaded) doc["always_loaded"] = true;
    return doc;
}

std::string entity_file_name(std::uint64_t id) {
    return std::to_string(id) + ".ent";
}

CellAssignment assign_cells(const MapDesc& map, std::span<const MapEntity> entities) {
    CellAssignment out;
    std::map<std::tuple<std::uint32_t, std::int32_t, std::int32_t>, std::vector<std::size_t>> cells;
    for (std::size_t i = 0; i < entities.size(); ++i) {
        const MapEntity& e = entities[i];
        if (e.always_loaded) {
            out.always_loaded.push_back(i);
            continue;
        }
        bool found = false;
        const auto g = static_cast<std::uint32_t>(grid_index(map, e.grid, &found));
        const double size = grid_of(map, {g, 0, 0}).cell_size;
        cells[{g, cell_coord(e.position[2], size), cell_coord(e.position[0], size)}].push_back(i);
    }
    out.cells.reserve(cells.size());
    for (auto& [k, list] : cells) {
        out.cells.push_back({CellKey{std::get<0>(k), std::get<2>(k), std::get<1>(k)}, std::move(list)});
    }
    return out;
}

std::string cell_name(const MapDesc& map, CellKey key) {
    return grid_of(map, key).name + "_" + std::to_string(key.x) + "_" + std::to_string(key.z);
}

double cell_min_x(const MapDesc& map, CellKey key) {
    return key.x * grid_of(map, key).cell_size;
}

double cell_min_z(const MapDesc& map, CellKey key) {
    return key.z * grid_of(map, key).cell_size;
}

double cell_distance(const MapDesc& map, CellKey key, double x, double z) {
    const double size = grid_of(map, key).cell_size;
    const double x0 = cell_min_x(map, key);
    const double z0 = cell_min_z(map, key);
    const double dx = std::max({x0 - x, 0.0, x - (x0 + size)});
    const double dz = std::max({z0 - z, 0.0, z - (z0 + size)});
    return std::sqrt(dx * dx + dz * dz);
}

CookedMap cook_map(const MapSource& source) {
    CookedMap out;
    const CellAssignment assignment = assign_cells(source.map, source.entities);
    auto cell_document = [&](const std::vector<std::size_t>& members) {
        Json doc = Json::object();
        doc["format"] = "astral.cell";
        doc["version"] = kVersion;
        Json list = Json::array();
        for (std::size_t i : members) list.push_back(make_entity_file(source.entities[i]));
        doc["entities"] = std::move(list);
        return doc;
    };
    out.index = write_map(source.map);
    out.index["always_loaded"] = static_cast<std::uint64_t>(assignment.always_loaded.size());
    out.cells.push_back({"always", cell_document(assignment.always_loaded)});
    Json cells = Json::array();
    for (const Cell& cell : assignment.cells) {
        const std::string name = cell_name(source.map, cell.key);
        Json j = Json::object();
        j["name"] = name;
        j["grid"] = cell.key.grid;
        j["x"] = cell.key.x;
        j["z"] = cell.key.z;
        j["entities"] = static_cast<std::uint64_t>(cell.entities.size());
        cells.push_back(std::move(j));
        out.cells.push_back({name, cell_document(cell.entities)});
    }
    out.index["cells"] = std::move(cells);
    return out;
}

std::vector<MapEntity> read_cell(const MapDesc& map, const Json& cell) {
    if (!check_format(cell, "astral.cell", nullptr)) return {};
    std::vector<EntityFile> files;
    for (const Json& e : cell["entities"].as_array()) files.push_back({"cell", e});
    return read_map_source(map, files).entities;
}

} // namespace astral
