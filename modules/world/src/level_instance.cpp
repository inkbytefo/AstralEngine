#include <astral/world/level_instance.hpp>
#include <astral/world/transform.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace astral {

namespace {

void merge_json(Json& base, const Json& patch) {
    if (base.is_object() && patch.is_object()) {
        for (const auto& [k, v] : patch.as_object()) merge_json(base[k], v);
        return;
    }
    base = patch;
}

Vec3 read_vec3(const Json& arr, Vec3 fallback) {
    if (!arr.is_array() || arr.size() != 3) return fallback;
    return {static_cast<float>(arr[0].as_number(fallback.x)), static_cast<float>(arr[1].as_number(fallback.y)),
            static_cast<float>(arr[2].as_number(fallback.z))};
}

Quat read_quat(const Json& arr) {
    if (!arr.is_array() || arr.size() != 4) return {};
    return normalize(Quat{static_cast<float>(arr[0].as_number(0.0)), static_cast<float>(arr[1].as_number(0.0)),
                          static_cast<float>(arr[2].as_number(0.0)), static_cast<float>(arr[3].as_number(1.0))});
}

Json write_vec3(Vec3 v) {
    Json a = Json::array();
    a.push_back(static_cast<double>(v.x));
    a.push_back(static_cast<double>(v.y));
    a.push_back(static_cast<double>(v.z));
    return a;
}

Json write_quat(Quat q) {
    Json a = Json::array();
    a.push_back(static_cast<double>(q.x));
    a.push_back(static_cast<double>(q.y));
    a.push_back(static_cast<double>(q.z));
    a.push_back(static_cast<double>(q.w));
    return a;
}

struct WorldPose {
    std::array<double, 3> position{0.0, 0.0, 0.0};
    Quat rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

WorldPose compose_pose(const WorldPose& outer, const Json& local_tf) {
    const Vec3 lp = read_vec3(local_tf["position"], {0.0f, 0.0f, 0.0f});
    const Quat lr = read_quat(local_tf["rotation"]);
    const Vec3 ls = read_vec3(local_tf["scale"], {1.0f, 1.0f, 1.0f});
    const Vec3 scaled{lp.x * outer.scale.x, lp.y * outer.scale.y, lp.z * outer.scale.z};
    const Vec3 rotated = outer.rotation * scaled;
    WorldPose out;
    out.position = {outer.position[0] + static_cast<double>(rotated.x),
                    outer.position[1] + static_cast<double>(rotated.y),
                    outer.position[2] + static_cast<double>(rotated.z)};
    out.rotation = normalize(outer.rotation * lr);
    out.scale = {outer.scale.x * ls.x, outer.scale.y * ls.y, outer.scale.z * ls.z};
    return out;
}

struct Expander {
    const LevelInstancePlacement& root;
    const TypeRegistry& types;
    const DocumentSource& docs;
    const LevelInstanceLimits& limits;
    LevelInstanceExpandResult result;
    std::vector<std::string> stack;
    std::unordered_set<std::uint64_t> used_ids;

    std::uint64_t alloc_id(std::string_view local_path) {
        std::uint64_t id = level_instance_entity_id(root.instance_id, local_path);
        while (id == 0 || !used_ids.insert(id).second) {
            id = (id % 0x7FFFFFFEu) + 1u;
        }
        return id;
    }

    bool is_deleted(std::string_view local_path) const {
        return std::find(root.deleted.begin(), root.deleted.end(), local_path) != root.deleted.end();
    }

    void expand_record(const Json& rec, const WorldPose& pose, const std::string& path_prefix, std::uint32_t depth) {
        const auto rec_id = static_cast<std::uint32_t>(rec["id"].as_number(0.0));
        if (rec_id == 0) return;
        const std::string local_path = path_prefix.empty() ? std::to_string(rec_id) : path_prefix + "/" + std::to_string(rec_id);
        if (is_deleted(local_path)) return;

        if (const Json* nested = rec.find("level_instance"); nested && nested->is_object()) {
            const std::string sub_scene = (*nested)["scene"].as_string();
            Json nested_tf = Json::object();
            if (const Json* p = nested->find("position")) nested_tf["position"] = *p;
            if (const Json* r = nested->find("rotation")) nested_tf["rotation"] = *r;
            if (const Json* s = nested->find("scale")) nested_tf["scale"] = *s;
            expand_scene(sub_scene, compose_pose(pose, nested_tf), local_path, depth + 1);
            return;
        }

        Json expanded_rec = rec;
        Json comps = expanded_rec["components"].is_object() ? expanded_rec["components"] : Json::object();
        if (const Json* ov = root.overrides.find(local_path); ov && ov->is_object()) {
            if (const Json* ov_comps = ov->find("components"); ov_comps && ov_comps->is_object()) {
                merge_json(comps, *ov_comps);
            }
            if (const Json* rem = ov->find("removed"); rem && rem->is_array()) {
                for (const Json& name : rem->as_array()) comps.erase(name.as_string());
            }
        }

        // Apply placement transform to root entities (no artificial Parent chain).
        WorldPose entity_pose = pose;
        if (comps.find("astral::Parent") == nullptr) {
            const Json local_tf = comps.find("astral::Transform") ? comps["astral::Transform"] : Json::object();
            entity_pose = compose_pose(pose, local_tf);
            Json world_tf = Json::object();
            Json wp = Json::array();
            wp.push_back(entity_pose.position[0]);
            wp.push_back(entity_pose.position[1]);
            wp.push_back(entity_pose.position[2]);
            world_tf["position"] = std::move(wp);
            world_tf["rotation"] = write_quat(entity_pose.rotation);
            world_tf["scale"] = write_vec3(entity_pose.scale);
            comps["astral::Transform"] = std::move(world_tf);
        }

        Json member = Json::object();
        member["instance_id"] = static_cast<double>(root.instance_id);
        member["source_scene"] = root.scene_path;
        member["local_path"] = local_path;
        comps["astral::LevelInstanceMember"] = std::move(member);

        const std::uint64_t world_id = alloc_id(local_path);
        expanded_rec["id"] = static_cast<double>(world_id);
        expanded_rec["components"] = std::move(comps);

        MapEntity me;
        me.id = world_id;
        me.record = std::move(expanded_rec);
        me.grid = root.grid;
        me.layers = root.layers;
        me.always_loaded = root.always_loaded;
        me.position = entity_pose.position;
        result.entities.push_back(std::move(me));
    }

    void expand_scene(const std::string& scene_path, const WorldPose& pose, const std::string& path_prefix, std::uint32_t depth) {
        if (depth > limits.max_depth) {
            result.errors.push_back("level instance " + std::to_string(root.instance_id) + ": exceeded max nesting depth (" +
                                    std::to_string(limits.max_depth) + ") at \"" + scene_path + "\"");
            return;
        }
        if (std::find(stack.begin(), stack.end(), scene_path) != stack.end()) {
            std::string chain;
            for (const std::string& s : stack) chain += s + " -> ";
            result.errors.push_back("level instance cycle detected: " + chain + scene_path);
            return;
        }
        const SourceDocument src = docs(scene_path);
        if (!src.json || !src.json->is_object()) {
            result.errors.push_back("level instance \"" + scene_path + "\": " + (src.error.empty() ? "not found" : src.error));
            return;
        }
        if ((*src.json)["format"].as_string() != "astral.scene") {
            result.errors.push_back("level instance \"" + scene_path + "\": not an astral.scene document");
            return;
        }
        if (std::find(result.sources.begin(), result.sources.end(), scene_path) == result.sources.end()) {
            result.sources.push_back(scene_path);
        }

        stack.push_back(scene_path);
        const Json& list = (*src.json)["entities"];
        for (std::size_t i = 0; i < list.size(); ++i) {
            if (result.entities.size() >= limits.max_entities) {
                result.truncated = true;
                result.errors.push_back("level instance " + std::to_string(root.instance_id) + ": exceeded max entity limit (" +
                                        std::to_string(limits.max_entities) + ")");
                break;
            }
            expand_record(list[i], pose, path_prefix, depth);
        }
        stack.pop_back();
    }
};

} // namespace

std::uint64_t level_instance_entity_id(std::uint64_t instance_id, std::string_view local_path) noexcept {
    std::uint64_t h = 14695981039346656037ull;
    auto mix = [&](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            h ^= (v >> (i * 8)) & 0xFFu;
            h *= 1099511628211ull;
        }
    };
    mix(instance_id);
    for (unsigned char c : local_path) {
        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ull;
    }
    // Keep within 1 .. 2^31-1 so it fits MapEntity::id and JSON exact integer range.
    const std::uint64_t id = (h & 0x7FFFFFFFull);
    return id == 0 ? 1u : id;
}

Json make_level_instance_entity_file(const LevelInstancePlacement& p) {
    Json doc = Json::object();
    doc["format"] = "astral.entity";
    doc["version"] = 1.0;

    Json rec = Json::object();
    rec["id"] = static_cast<double>(p.instance_id);
    Json li = Json::object();
    li["scene"] = p.scene_path;
    Json pos = Json::array();
    pos.push_back(p.position[0]);
    pos.push_back(p.position[1]);
    pos.push_back(p.position[2]);
    li["position"] = std::move(pos);
    li["rotation"] = write_quat(p.rotation);
    li["scale"] = write_vec3(p.scale);
    if (p.overrides.is_object() && p.overrides.size() > 0) li["overrides"] = p.overrides;
    if (!p.deleted.empty()) {
        Json del = Json::array();
        for (const std::string& d : p.deleted) del.push_back(d);
        li["deleted"] = std::move(del);
    }
    rec["level_instance"] = std::move(li);
    doc["entity"] = std::move(rec);
    if (!p.grid.empty()) doc["grid"] = p.grid;
    if (!p.layers.empty()) {
        Json layers = Json::array();
        for (const std::string& l : p.layers) layers.push_back(l);
        doc["layers"] = std::move(layers);
    }
    if (p.always_loaded) doc["always_loaded"] = true;
    return doc;
}

std::optional<LevelInstancePlacement> parse_level_instance_entity(const Json& entity_doc) {
    if (!entity_doc.is_object()) return std::nullopt;
    const Json* rec = entity_doc.find("entity");
    if (!rec || !rec->is_object()) rec = &entity_doc;
    const Json* li = rec->find("level_instance");
    if (!li || !li->is_object()) return std::nullopt;

    LevelInstancePlacement out;
    out.instance_id = static_cast<std::uint64_t>((*rec)["id"].as_number(0.0));
    out.scene_path = (*li)["scene"].as_string();
    if (out.instance_id == 0 || out.scene_path.empty()) return std::nullopt;
    if (const Json* pos = li->find("position"); pos && pos->is_array() && pos->size() == 3) {
        out.position = {(*pos)[0].as_number(0.0), (*pos)[1].as_number(0.0), (*pos)[2].as_number(0.0)};
    }
    if (const Json* rot = li->find("rotation")) out.rotation = read_quat(*rot);
    if (const Json* scl = li->find("scale")) out.scale = read_vec3(*scl, {1.0f, 1.0f, 1.0f});
    if (const Json* ov = li->find("overrides"); ov && ov->is_object()) out.overrides = *ov;
    if (const Json* del = li->find("deleted"); del && del->is_array()) {
        for (const Json& d : del->as_array()) out.deleted.push_back(d.as_string());
    }
    out.grid = entity_doc["grid"].as_string();
    if (const Json* layers = entity_doc.find("layers"); layers && layers->is_array()) {
        for (const Json& l : layers->as_array()) out.layers.push_back(l.as_string());
    }
    out.always_loaded = entity_doc["always_loaded"].as_bool(false);
    return out;
}

LevelInstanceExpandResult expand_level_instance(const LevelInstancePlacement& placement,
                                                const TypeRegistry& types,
                                                const DocumentSource& docs,
                                                const LevelInstanceLimits& limits) {
    Expander expander{placement, types, docs, limits, {}, {}, {}};
    const WorldPose root_pose{placement.position, placement.rotation, placement.scale};
    expander.expand_scene(placement.scene_path, root_pose, {}, 0);
    return std::move(expander.result);
}

MapSource expand_map_level_instances(const MapSource& source, const TypeRegistry& types,
                                     const DocumentSource& docs, const LevelInstanceLimits& limits) {
    MapSource out;
    out.map = source.map;
    out.errors = source.errors;
    for (const MapEntity& me : source.entities) {
        if (const Json* li = me.record.find("level_instance"); li && li->is_object()) {
            Json wrapper = make_entity_file(me);
            auto placement = parse_level_instance_entity(wrapper);
            if (!placement) {
                out.errors.push_back("entity " + std::to_string(me.id) + ": malformed level_instance");
                continue;
            }
            LevelInstanceExpandResult expanded = expand_level_instance(*placement, types, docs, limits);
            out.errors.insert(out.errors.end(), expanded.errors.begin(), expanded.errors.end());
            for (MapEntity& inner : expanded.entities) out.entities.push_back(std::move(inner));
        } else {
            out.entities.push_back(me);
        }
    }
    std::sort(out.entities.begin(), out.entities.end(),
              [](const MapEntity& a, const MapEntity& b) { return a.id < b.id; });
    return out;
}

void set_level_instance_override(LevelInstancePlacement& placement, std::string_view local_path,
                                 std::string_view component, std::string_view field, const Json& value) {
    if (!placement.overrides.is_object()) placement.overrides = Json::object();
    Json& entry = placement.overrides[local_path];
    if (!entry.is_object()) entry = Json::object();
    Json& comps = entry["components"];
    if (!comps.is_object()) comps = Json::object();
    Json& comp = comps[component];
    if (!comp.is_object()) comp = Json::object();
    comp[field] = value;
}

void register_level_instance_types(TypeRegistry& types) {
    types.add<LevelInstanceMember>();
    types.add<CrossCellRef>();
}

} // namespace astral
