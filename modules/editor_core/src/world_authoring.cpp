#include <astral/editor/world_authoring.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_map>

namespace astral::editor {

namespace {

std::filesystem::path entities_folder_for(const std::filesystem::path& world_path) {
    std::string p = world_path.generic_string();
    if (p.ends_with(".world")) p.resize(p.size() - 6);
    return std::filesystem::path(p + ".entities");
}

PlacementCategory category_from_string(std::string_view name) {
    std::string lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "road") return PlacementCategory::Road;
    if (lower == "building") return PlacementCategory::Building;
    if (lower == "foliage") return PlacementCategory::Foliage;
    if (lower == "rock") return PlacementCategory::Rock;
    if (lower == "level_instance" || lower == "levelinstance") return PlacementCategory::LevelInstance;
    return PlacementCategory::Prefab;
}

Json make_transform_json(double x, double y, double z, Vec3 scale) {
    Json tf = Json::object();
    Json pos = Json::array();
    pos.push_back(x);
    pos.push_back(y);
    pos.push_back(z);
    tf["position"] = std::move(pos);
    Json rot = Json::array();
    rot.push_back(0.0);
    rot.push_back(0.0);
    rot.push_back(0.0);
    rot.push_back(1.0);
    tf["rotation"] = std::move(rot);
    Json scl = Json::array();
    scl.push_back(static_cast<double>(scale.x));
    scl.push_back(static_cast<double>(scale.y));
    scl.push_back(static_cast<double>(scale.z));
    tf["scale"] = std::move(scl);
    return tf;
}

} // namespace

std::string_view placement_category_name(PlacementCategory cat) noexcept {
    switch (cat) {
    case PlacementCategory::Road: return "road";
    case PlacementCategory::Building: return "building";
    case PlacementCategory::Foliage: return "foliage";
    case PlacementCategory::Rock: return "rock";
    case PlacementCategory::Prefab: return "prefab";
    case PlacementCategory::LevelInstance: return "level_instance";
    }
    return "prefab";
}

std::vector<PlacementToolDef> default_placement_tools() {
    return {
        {"road_segment", "Road Segment", PlacementCategory::Road, "", "main", {}, {6.0f, 0.2f, 8.0f}, 8.0, 0.05},
        {"building_house", "Building", PlacementCategory::Building, "prefabs/house.prefab", "main", {}, {1.0f, 1.0f, 1.0f}, 12.0, 0.0},
        {"foliage_pine", "Foliage (Pine)", PlacementCategory::Foliage, "prefabs/pine.prefab", "main", {}, {1.0f, 1.0f, 1.0f}, 6.0, 0.0},
        {"rock_boulder", "Rock Boulder", PlacementCategory::Rock, "prefabs/rock.prefab", "main", {}, {1.5f, 1.2f, 1.5f}, 5.0, 0.0},
        {"prefab_prop", "Prefab Prop", PlacementCategory::Prefab, "prefabs/lamp.prefab", "main", {}, {1.0f, 1.0f, 1.0f}, 4.0, 0.0},
    };
}

std::vector<PlacementToolDef> parse_placement_tools(const Json& doc, std::string* error) {
    std::vector<PlacementToolDef> out;
    const Json* arr = doc.is_array() ? &doc : doc.find("tools");
    if (!arr || !arr->is_array()) {
        if (error) *error = "placement tools JSON must be an array or an object with a \"tools\" array";
        return out;
    }
    for (std::size_t i = 0; i < arr->size(); ++i) {
        const Json& item = (*arr)[i];
        if (!item.is_object()) continue;
        PlacementToolDef def;
        def.id = item["id"].as_string();
        def.label = item["label"].as_string();
        if (def.id.empty()) continue;
        if (def.label.empty()) def.label = def.id;
        def.category = category_from_string(item["category"].as_string());
        def.asset_path = item["asset_path"].as_string();
        def.default_grid = item["grid"].as_string();
        if (const Json* layers = item.find("layers"); layers && layers->is_array()) {
            for (const Json& l : layers->as_array()) def.layers.push_back(l.as_string());
        }
        if (const Json* scl = item.find("scale"); scl && scl->is_array() && scl->size() == 3) {
            def.default_scale = {static_cast<float>((*scl)[0].as_number(1.0)),
                                 static_cast<float>((*scl)[1].as_number(1.0)),
                                 static_cast<float>((*scl)[2].as_number(1.0))};
        }
        def.spacing = item["spacing"].as_number(8.0);
        def.y_offset = item["y_offset"].as_number(0.0);
        out.push_back(std::move(def));
    }
    return out;
}

WorldAuthoringSession::WorldAuthoringSession(IEditorFileSystem& fs, const TypeRegistry& types, std::filesystem::path asset_root)
    : fs_(fs), types_(types), asset_root_(std::move(asset_root)) {}

std::filesystem::path WorldAuthoringSession::resolve_asset(std::string_view rel) const {
    const std::filesystem::path p(rel);
    return (p.is_absolute() || asset_root_.empty()) ? p : (asset_root_ / p);
}

bool WorldAuthoringSession::create_map(const std::filesystem::path& world_path, const MapDesc& desc, std::string* error) {
    MapDesc clean = desc;
    if (clean.grids.empty()) clean.grids.push_back(GridDesc{});
    if (!fs_.write_atomic(world_path, serialize_document(write_map(clean)), error)) return false;
    world_path_ = world_path;
    entities_dir_ = entities_folder_for(world_path);
    std::error_code dir_ec;
    std::filesystem::create_directories(entities_dir_, dir_ec);
    map_ = std::move(clean);
    raw_entities_.clear();
    open_ = true;
    audit_.record({0, "user", "create_map", "Created .world map", world_path_.string(), 0, 1, 0, true, {}});
    return true;
}

bool WorldAuthoringSession::open_map(const std::filesystem::path& world_path, std::string* error) {
    const auto text = fs_.read_text(world_path, error);
    if (!text) return false;
    std::string parse_err;
    const auto doc = Json::parse(*text, &parse_err);
    if (!doc) {
        if (error) *error = world_path.string() + ": " + parse_err;
        return false;
    }
    auto parsed = parse_map(*doc, error);
    if (!parsed) return false;

    const std::filesystem::path ent_dir = entities_folder_for(world_path);
    std::vector<EntityFile> files;
    for (const std::filesystem::path& file : fs_.list_files(ent_dir, {".ent"})) {
        std::string f_err;
        if (auto ftext = fs_.read_text(file, &f_err)) {
            if (auto fdoc = Json::parse(*ftext, &f_err)) files.push_back({file.filename().string(), std::move(*fdoc)});
        }
    }
    MapSource src = read_map_source(*parsed, files);
    world_path_ = world_path;
    entities_dir_ = ent_dir;
    map_ = std::move(*parsed);
    raw_entities_ = std::move(src.entities);
    open_ = true;
    return true;
}

std::uint64_t WorldAuthoringSession::next_entity_id() const noexcept {
    std::uint64_t hi = 0;
    for (const MapEntity& e : raw_entities_) hi = std::max(hi, e.id);
    return hi + 1;
}

bool WorldAuthoringSession::save_map_desc(const MapDesc& updated, std::string* error) {
    if (!open_) return error ? (*error = "no map open", false) : false;
    if (!fs_.write_atomic(world_path_, serialize_document(write_map(updated)), error)) return false;
    map_ = updated;
    audit_.record({0, "user", "save_map", "Updated .world descriptor", world_path_.string(), 0, 0, 1, true, {}});
    return true;
}

bool WorldAuthoringSession::write_entity_file(std::uint64_t id, const Json& doc, std::string* error) {
    const std::filesystem::path file = entities_dir_ / entity_file_name(id);
    return fs_.write_atomic(file, serialize_document(doc), error);
}

bool WorldAuthoringSession::upsert_entity(const MapEntity& entity, std::string* error) {
    if (!open_) return error ? (*error = "no map open", false) : false;
    if (entity.id == 0) return error ? (*error = "entity id must be > 0", false) : false;
    const Json doc = make_entity_file(entity);
    if (!write_entity_file(entity.id, doc, error)) return false;
    auto it = std::find_if(raw_entities_.begin(), raw_entities_.end(),
                           [&](const MapEntity& e) { return e.id == entity.id; });
    if (it != raw_entities_.end()) *it = entity;
    else raw_entities_.push_back(entity);
    std::sort(raw_entities_.begin(), raw_entities_.end(), [](const MapEntity& a, const MapEntity& b) { return a.id < b.id; });
    audit_.record({0, "user", "upsert_entity", "Saved " + entity_file_name(entity.id),
                   (entities_dir_ / entity_file_name(entity.id)).string(), 0, 0, 1, true, {}});
    return true;
}

bool WorldAuthoringSession::place_level_instance(const LevelInstancePlacement& placement, std::string* error) {
    if (!open_) return error ? (*error = "no map open", false) : false;
    if (placement.instance_id == 0 || placement.scene_path.empty()) {
        return error ? (*error = "invalid level instance placement", false) : false;
    }
    const Json doc = make_level_instance_entity_file(placement);
    if (!write_entity_file(placement.instance_id, doc, error)) return false;
    const std::vector<EntityFile> single{{entity_file_name(placement.instance_id), doc}};
    const MapSource parsed = read_map_source(map_, single);
    if (parsed.entities.empty()) return error ? (*error = "failed to parse level instance entity", false) : false;
    return upsert_entity(parsed.entities.front(), error);
}

bool WorldAuthoringSession::edit_instance_override(std::uint64_t instance_id, std::string_view local_path,
                                                   std::string_view component, std::string_view field, const Json& value,
                                                   std::string* error) {
    if (!open_) return error ? (*error = "no map open", false) : false;
    auto it = std::find_if(raw_entities_.begin(), raw_entities_.end(),
                           [&](const MapEntity& e) { return e.id == instance_id; });
    if (it == raw_entities_.end()) return error ? (*error = "level instance not found", false) : false;
    auto placement = parse_level_instance_entity(make_entity_file(*it));
    if (!placement) return error ? (*error = "entity is not a level instance", false) : false;
    set_level_instance_override(*placement, local_path, component, field, value);
    if (!place_level_instance(*placement, error)) return false;
    audit_.record({0, "user", "instance_override",
                   "Override " + std::to_string(instance_id) + "/" + std::string(local_path) + "." + std::string(field),
                   (entities_dir_ / entity_file_name(instance_id)).string(), 0, 0, 1, true, {}});
    return true;
}

bool WorldAuthoringSession::edit_source_scene_in_context(std::string_view scene_path, std::uint32_t record_id,
                                                         std::string_view component, std::string_view field,
                                                         const Json& value, std::string* error) {
    const std::filesystem::path full = resolve_asset(scene_path);
    EditorSession scene_session(fs_, types_, asset_root_);
    if (!scene_session.open(full, error)) return false;
    Transaction tx;
    tx.author = "user";
    tx.summary = "Edit-in-context " + std::string(scene_path) + " #" + std::to_string(record_id);
    tx.expected_revision = scene_session.document().revision;
    tx.commands.push_back({.kind = CommandKind::SetComponentField, .target = {record_id, ""},
                           .component = std::string(component), .field = std::string(field), .value = value});
    const TransactionResult res = scene_session.apply(tx);
    if (!res.ok) {
        if (error) *error = res.error;
        return false;
    }
    audit_.record({0, "user", "edit_in_context", tx.summary, full.string(), 0, res.new_revision.number, 1, true, {}});
    return true;
}

std::optional<std::uint64_t> WorldAuthoringSession::place_with_tool(const PlacementToolDef& tool, double world_x,
                                                                    double world_z, const IHeightSampler& height_sampler,
                                                                    std::string* error) {
    if (!open_) return error ? (*error = "no map open", std::nullopt) : std::nullopt;
    const double world_y = height_sampler.sample_height(world_x, world_z) + tool.y_offset;
    const std::uint64_t id = next_entity_id();
    if (tool.category == PlacementCategory::LevelInstance) {
        LevelInstancePlacement p{.instance_id = id, .scene_path = tool.asset_path,
                                 .position = {world_x, world_y, world_z}, .scale = tool.default_scale,
                                 .grid = tool.default_grid, .layers = tool.layers};
        return place_level_instance(p, error) ? std::optional<std::uint64_t>(id) : std::nullopt;
    }
    MapEntity e;
    e.id = id;
    e.grid = tool.default_grid.empty() && !map_.grids.empty() ? map_.grids.front().name : tool.default_grid;
    e.layers = tool.layers;
    e.position = {world_x, world_y, world_z};
    Json rec = Json::object();
    rec["id"] = static_cast<double>(id);
    if (!tool.asset_path.empty()) rec["prefab"] = tool.asset_path;
    Json comps = Json::object();
    Json name = Json::object();
    name["value"] = tool.label + "_" + std::to_string(id);
    comps["astral::Name"] = std::move(name);
    comps["astral::Transform"] = make_transform_json(world_x, world_y, world_z, tool.default_scale);
    rec["components"] = std::move(comps);
    e.record = std::move(rec);
    return upsert_entity(e, error) ? std::optional<std::uint64_t>(id) : std::nullopt;
}

std::vector<std::uint64_t> WorldAuthoringSession::place_spline_with_tool(const PlacementToolDef& tool,
                                                                         std::span<const std::array<double, 2>> points,
                                                                         const IHeightSampler& height_sampler,
                                                                         std::string* error) {
    std::vector<std::uint64_t> placed;
    if (points.size() < 2) return placed;
    const double step = std::max(tool.spacing, 1.0);
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        const double dx = points[i + 1][0] - points[i][0];
        const double dz = points[i + 1][1] - points[i][1];
        const double dist = std::sqrt(dx * dx + dz * dz);
        const int count = std::max(1, static_cast<int>(std::floor(dist / step)));
        for (int k = 0; k < count; ++k) {
            const double t = static_cast<double>(k) / static_cast<double>(count);
            if (auto id = place_with_tool(tool, points[i][0] + dx * t, points[i][1] + dz * t, height_sampler, error)) {
                placed.push_back(*id);
            }
        }
    }
    return placed;
}

MapSource WorldAuthoringSession::build_expanded_source(std::vector<std::string>* errors) const {
    MapSource raw;
    raw.map = map_;
    raw.entities = raw_entities_;
    std::unordered_map<std::string, Json> cache;
    const DocumentSource loader = [&](std::string_view rel) -> SourceDocument {
        const std::string key(rel);
        if (auto it = cache.find(key); it != cache.end()) return {&it->second, 1, {}};
        std::string err;
        auto text = fs_.read_text(resolve_asset(rel), &err);
        if (!text) return {nullptr, 0, err};
        auto parsed = Json::parse(*text, &err);
        if (!parsed) return {nullptr, 0, err};
        auto [ins, _] = cache.emplace(key, std::move(*parsed));
        return {&ins->second, 1, {}};
    };
    MapSource expanded = expand_map_level_instances(raw, types_, loader);
    if (errors) errors->insert(errors->end(), expanded.errors.begin(), expanded.errors.end());
    return expanded;
}

CookedMap WorldAuthoringSession::cook_current_map(std::vector<std::string>* errors) const {
    return cook_map(build_expanded_source(errors));
}

std::vector<CellOwnershipInfo> WorldAuthoringSession::inspect_cells(std::vector<std::string>* errors) const {
    const MapSource expanded = build_expanded_source(errors);
    const CellAssignment assigned = assign_cells(expanded.map, expanded.entities);
    std::vector<CellOwnershipInfo> out;
    out.reserve(assigned.cells.size());
    for (const Cell& c : assigned.cells) {
        const double size = c.key.grid < map_.grids.size() ? map_.grids[c.key.grid].cell_size : 128.0;
        out.push_back({c.key, cell_name(map_, c.key), cell_min_x(map_, c.key), cell_min_z(map_, c.key), size, c.entities.size()});
    }
    return out;
}

} // namespace astral::editor
