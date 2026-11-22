#include "scene_internal.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace astral::scene_detail {

namespace {

/// Generation of the stand-in entities remap() hands out while rewriting references.
constexpr std::uint32_t placeholder_generation = 0xFFFFFFFFu;

/// Objects merge member by member; anything else replaces (null is a value, not a deletion).
void merge(Json& base, const Json& patch) {
    if (base.is_object() && patch.is_object()) {
        for (const auto& [key, value] : patch.as_object()) merge(base[key], value);
        return;
    }
    base = patch;
}

/// The smallest patch that turns `base` into `actual`; nullopt when they are equal.
std::optional<Json> diff(const Json& base, const Json& actual) {
    if (base == actual) return std::nullopt;
    if (base.is_object() && actual.is_object()) {
        Json patch = Json::object();
        for (const auto& [key, value] : actual.as_object()) {
            const Json* b = base.find(key);
            if (!b) patch[key] = value;
            else if (auto d = diff(*b, value)) patch[key] = std::move(*d);
        }
        if (patch.size() == 0) return std::nullopt;
        return patch;
    }
    return actual;
}

/// The members of `full` that `shape` names (recursively): a transcoded patch, still partial.
Json restrict_to(const Json& full, const Json& shape) {
    if (!full.is_object() || !shape.is_object()) return full;
    Json out = Json::object();
    for (const auto& [key, value] : shape.as_object()) {
        if (const Json* f = full.find(key)) out[key] = restrict_to(*f, value);
    }
    return out;
}

/// Applies {"components": {...}, "removed": [...]} to a part's components.
void apply_patches(const TypeRegistry& types, Json& target, const Json& record, const RefMap& map,
                   std::vector<std::string>* errors, const std::string& label) {
    for (const auto& [name, patch] : record["components"].as_object()) {
        const TypeRegistry::Entry* t = types.find(name);
        if (!t) {
            warn(errors, label + ": " + name + ": unknown component (not registered)");
            continue;
        }
        Json full = remap(*t, patch, map, errors, label + ": " + name);
        if (target.find(name)) merge(target[name], restrict_to(full, patch));
        else target[name] = std::move(full); // added by the instance: the whole component
    }
    for (const Json& name : record["removed"].as_array()) target.erase(name.as_string());
}

} // namespace

std::string normalize_path(std::string_view path) {
    std::string out;
    out.reserve(path.size());
    for (char c : path) out += c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    while (out.starts_with("./")) out.erase(0, 2);
    return out;
}

std::optional<std::uint32_t> record_id(const Json& j) {
    if (!j.is_number()) return std::nullopt;
    const double d = j.as_number();
    if (d < 0 || d > 4294967295.0 || d != std::floor(d)) return std::nullopt;
    return static_cast<std::uint32_t>(d);
}

std::optional<std::pair<std::uint32_t, std::string>> parse_ref(const Json& j) {
    if (auto id = record_id(j)) return std::pair{*id, std::string()};
    if (!j.is_string()) return std::nullopt;
    const std::string& s = j.as_string();
    const std::size_t slash = s.find('/');
    const std::string head = s.substr(0, slash);
    if (head.empty() || head.size() > 10 || !std::all_of(head.begin(), head.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        return std::nullopt;
    }
    const unsigned long long id = std::stoull(head);
    if (id > 0xFFFFFFFFull) return std::nullopt;
    return std::pair{static_cast<std::uint32_t>(id), slash == std::string::npos ? std::string() : s.substr(slash + 1)};
}

Json placed_ref(std::uint32_t id, const std::string& rest) {
    return rest.empty() ? Json(id) : Json(key_of(id) + "/" + rest);
}

std::string ref_key(const Json& j) {
    if (auto id = record_id(j)) return key_of(*id);
    return j.is_string() ? j.as_string() : std::string();
}

std::string raw_key(Entity e) { return "#" + std::to_string(e.index) + "." + std::to_string(e.generation); }

std::optional<Entity> parse_raw(std::string_view key) {
    if (!key.starts_with('#') || key.find('/') != std::string_view::npos) return std::nullopt;
    const std::size_t dot = key.find('.');
    if (dot == std::string_view::npos) return std::nullopt;
    try {
        return Entity{static_cast<std::uint32_t>(std::stoul(std::string(key.substr(1, dot - 1)))),
                      static_cast<std::uint32_t>(std::stoul(std::string(key.substr(dot + 1))))};
    } catch (...) {
        return std::nullopt;
    }
}

bool check_scene_format(const Json& doc, std::string* error) {
    if (doc["format"].as_string() != scene_format) {
        *error = "not an astral.scene document";
        return false;
    }
    if (doc["version"].as_number(-1) != scene_version) {
        *error = "unsupported astral.scene version " + doc["version"].dump();
        return false;
    }
    return true;
}

Json remap(const TypeRegistry::Entry& t, const Json& value, const RefMap& map, std::vector<std::string>* errors,
           std::string label) {
    std::vector<Json> refs;
    LoadContext in;
    in.errors = errors;
    in.path = std::move(label);
    in.resolve = [&](const Json& r, Entity& out) {
        std::optional<Json> mapped = map ? map(r) : std::optional<Json>(r);
        if (!mapped) return false;
        refs.push_back(std::move(*mapped));
        out = Entity{static_cast<std::uint32_t>(refs.size() - 1), placeholder_generation};
        return true;
    };
    SaveContext out;
    out.ref = [&](Entity e) {
        return e.generation == placeholder_generation && e.index < refs.size() ? refs[e.index] : Json(nullptr);
    };
    return t.transcode(value, in, out);
}

std::vector<PlannedPart> plan_instance(const TypeRegistry& types, const PrefabTemplate& tpl, const Json* record,
                                       const Place& place, const RefMap& file_map, std::vector<std::string>* errors,
                                       const std::string& label) {
    const RefMap from_template = [&](const Json& r) -> std::optional<Json> {
        if (!r.is_string()) return std::nullopt;
        return place(r.as_string());
    };
    std::vector<PlannedPart> parts;
    parts.reserve(tpl.parts.size());
    for (const PrefabTemplate::Part& p : tpl.parts) {
        PlannedPart out{p.path, Json::object()};
        for (const auto& [name, value] : p.components.as_object()) {
            if (const TypeRegistry::Entry* t = types.find(name)) out.components[name] = remap(*t, value, from_template, errors, label);
        }
        parts.push_back(std::move(out));
    }
    if (!record) return parts;

    auto find = [&](std::string_view path) -> PlannedPart* {
        for (PlannedPart& p : parts) {
            if (p.path == path) return &p;
        }
        return nullptr;
    };
    apply_patches(types, parts[0].components, *record, file_map, errors, label);
    for (const auto& [path, patch] : (*record)["parts"].as_object()) {
        PlannedPart* p = path.empty() ? nullptr : find(path);
        if (!p) {
            warn(errors, label + ": overrides part \"" + path + "\", which the prefab does not have");
            continue;
        }
        apply_patches(types, p->components, patch, file_map, errors, label + "/" + path);
    }

    // Deleted parts take their children (by Parent) with them.
    std::vector<Json> placed;
    for (const PlannedPart& p : parts) placed.push_back(place(p.path));
    std::vector<bool> dead(parts.size(), false);
    for (const Json& d : (*record)["deleted"].as_array()) {
        const std::string& path = d.as_string();
        bool found = false;
        for (std::size_t i = 1; i < parts.size(); ++i) {
            if (parts[i].path == path) dead[i] = found = true;
        }
        if (!found) warn(errors, label + ": deletes part \"" + path + "\", which the prefab does not have");
    }
    for (bool changed = true; changed;) {
        changed = false;
        for (std::size_t i = 1; i < parts.size(); ++i) {
            if (dead[i]) continue;
            const Json& parent = parts[i].components[parent_name]["entity"];
            if (parent.is_null()) continue;
            for (std::size_t j = 0; j < parts.size(); ++j) {
                if (dead[j] && placed[j] == parent) {
                    dead[i] = changed = true;
                    break;
                }
            }
        }
    }
    std::vector<PlannedPart> alive;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (!dead[i]) alive.push_back(std::move(parts[i]));
    }
    return alive;
}

bool Spawner::resolve(const Json& j, Entity& out) const {
    const std::string key = ref_key(j);
    if (auto it = refs.find(key); it != refs.end()) {
        out = it->second;
        return world.alive(out);
    }
    if (auto raw = parse_raw(key)) {
        out = *raw;
        return world.alive(out);
    }
    return false;
}

void Spawner::finish() {
    LoadContext ctx;
    ctx.errors = errors;
    ctx.resolve = [this](const Json& j, Entity& e) { return resolve(j, e); };
    for (Pending& p : pending) {
        for (const auto& [name, value] : p.components.as_object()) {
            const TypeRegistry::Entry* t = types.find(name);
            if (!t) {
                warn(errors, p.label + ": " + name + ": unknown component (not registered)");
                continue;
            }
            if (p.keep_existing && t->has(world, p.entity)) continue;
            LoadContext c = ctx;
            c.path = p.label + ": " + name;
            t->load(world, p.entity, value, c);
        }
    }
    pending.clear();
}

void Spawner::spawn(Entity root, const std::shared_ptr<const PrefabTemplate>& tpl, const Json* record, const Json& root_ref,
                    const RefMap& file_map, const std::string& label) {
    if (warned.insert(tpl.get()).second) {
        for (const std::string& w : tpl->warnings) warn(errors, w);
    }
    const std::string root_key = ref_key(root_ref);
    const Place place = [&](const std::string& t) { return t.empty() ? root_ref : Json(root_key + "/" + t); };
    std::vector<PlannedPart> parts = plan_instance(types, *tpl, record, place, file_map, errors, label);
    refs[root_key] = root;
    for (PlannedPart& p : parts) {
        if (p.path.empty()) {
            pending.push_back({root, std::move(p.components), true, label});
            continue;
        }
        const Entity e = world.create(PrefabPart{root, p.path});
        refs[root_key + "/" + p.path] = e;
        pending.push_back({e, std::move(p.components), false, label + "/" + p.path});
    }
    world.add(root, PrefabSpawned{tpl, {}});
}

std::vector<Entity> parts_of(World& world, Entity instance) {
    std::vector<Entity> out;
    world.each<const PrefabPart>([&](Entity e, const PrefabPart& p) {
        if (p.instance == instance) out.push_back(e);
    });
    return out;
}

Json compute_overrides(World& world, const TypeRegistry& types, Entity root, const PrefabTemplate& tpl, const Json& root_ref,
                       const std::function<Json(Entity)>& outer) {
    const std::string root_key = ref_key(root_ref);
    const Place place = [&](const std::string& t) { return t.empty() ? root_ref : Json(root_key + "/" + t); };
    std::unordered_map<std::string, Entity> actual;
    std::unordered_map<std::uint64_t, std::string> path_of;
    world.each<const PrefabPart>([&](Entity e, const PrefabPart& p) {
        if (p.instance != root) return;
        actual.emplace(p.path, e);
        path_of.emplace(entity_key(e), p.path);
    });
    SaveContext ctx;
    ctx.ref = [&](Entity e) -> Json {
        if (e == root) return root_ref;
        if (auto it = path_of.find(entity_key(e)); it != path_of.end()) return place(it->second);
        return outer ? outer(e) : Json(nullptr);
    };
    const std::vector<PlannedPart> base = plan_instance(types, tpl, nullptr, place, {}, nullptr, {});

    auto compare = [&](Entity e, const Json& expected) {
        Json record = Json::object();
        Json components = Json::object();
        Json removed = Json::array();
        for (const TypeRegistry::Entry& t : types.entries()) {
            const Json* b = expected.find(t.name);
            if (!t.has(world, e)) {
                if (b) removed.push_back(t.name);
                continue;
            }
            Json value = t.save(world, e, ctx);
            if (!b) components[t.name] = std::move(value);
            else if (auto d = diff(*b, value)) components[t.name] = std::move(*d);
        }
        if (components.size()) record["components"] = std::move(components);
        if (removed.size()) record["removed"] = std::move(removed);
        return record;
    };

    Json out = compare(root, base[0].components);
    Json parts = Json::object();
    std::vector<std::size_t> missing;
    for (std::size_t i = 1; i < base.size(); ++i) {
        auto it = actual.find(base[i].path);
        if (it == actual.end()) {
            missing.push_back(i);
            continue;
        }
        Json patch = compare(it->second, base[i].components);
        if (patch.size()) parts[base[i].path] = std::move(patch);
    }
    if (parts.size()) out["parts"] = std::move(parts);
    Json deleted = Json::array();
    for (std::size_t i : missing) {
        const Json& parent = base[i].components[parent_name]["entity"];
        const bool parent_gone = std::any_of(missing.begin(), missing.end(), [&](std::size_t j) {
            return !parent.is_null() && place(base[j].path) == parent;
        });
        if (!parent_gone) deleted.push_back(base[i].path); // children go with their parent anyway
    }
    if (deleted.size()) out["deleted"] = std::move(deleted);
    return out;
}

} // namespace astral::scene_detail

namespace astral {

using namespace scene_detail;

PrefabLibrary::PrefabLibrary(const TypeRegistry& types, DocumentSource source) : types_(types), source_(std::move(source)) {}

std::shared_ptr<const PrefabTemplate> PrefabLibrary::get(std::string_view path, std::string* error) {
    const std::string key = normalize_path(path);
    if (auto it = cache_.find(key); it != cache_.end()) return it->second;
    if (std::find(building_.begin(), building_.end(), key) != building_.end()) {
        if (error) {
            std::string chain;
            for (const std::string& b : building_) chain += b + " -> ";
            *error = "prefab includes itself: " + chain + key;
        }
        return nullptr;
    }
    building_.push_back(key);
    auto tpl = build(path, error);
    building_.pop_back();
    if (tpl) cache_.emplace(key, tpl);
    return tpl;
}

std::size_t PrefabLibrary::refresh() {
    std::size_t dropped = 0;
    for (auto it = cache_.begin(); it != cache_.end();) {
        const bool stale = std::any_of(it->second->sources.begin(), it->second->sources.end(), [&](const auto& s) {
            const SourceDocument d = source_(s.first);
            return !d.json || d.version != s.second;
        });
        if (stale) {
            it = cache_.erase(it);
            ++dropped;
        } else {
            ++it;
        }
    }
    return dropped;
}

std::shared_ptr<const PrefabTemplate> PrefabLibrary::build(std::string_view path, std::string* error) {
    auto fail = [&](const std::string& message) -> std::shared_ptr<const PrefabTemplate> {
        if (error) *error = std::string(path) + ": " + message;
        return nullptr;
    };
    const SourceDocument src = source_(path);
    if (!src.json) return fail(src.error.empty() ? "not found" : src.error);
    const Json& doc = *src.json;
    std::string format_error;
    if (!check_scene_format(doc, &format_error)) return fail(format_error);

    std::unordered_map<std::uint32_t, const Json*> by_id;
    std::vector<std::uint32_t> order;
    std::optional<std::uint32_t> root;
    for (const Json& record : doc["entities"].as_array()) {
        const auto id = record_id(record["id"]);
        if (!id) return fail("a record has no valid \"id\"");
        if (!by_id.emplace(*id, &record).second) return fail("duplicate id " + key_of(*id));
        order.push_back(*id);
        if (!record["components"].find(parent_name)) {
            if (root) return fail("a prefab needs exactly one root entity (one without astral::Parent); found " + key_of(*root) + " and " + key_of(*id));
            root = id;
        }
    }
    if (!root) return fail("a prefab needs exactly one root entity (one without astral::Parent)");
    const bool root_is_instance = by_id[*root]->find("prefab") != nullptr;

    // Paths inside the template: the root is "", record N is "N", part P of instance N is "N/P".
    // A variant's root is an instance whose parts keep their base prefab's paths.
    const RefMap file_map = [&](const Json& r) -> std::optional<Json> {
        const auto ref = parse_ref(r);
        if (!ref || !by_id.count(ref->first)) return std::nullopt;
        if (ref->first == *root) {
            if (!ref->second.empty() && !root_is_instance) return std::nullopt;
            return Json(ref->second);
        }
        return Json(ref->second.empty() ? key_of(ref->first) : key_of(ref->first) + "/" + ref->second);
    };

    auto tpl = std::make_shared<PrefabTemplate>();
    tpl->path = std::string(path);
    for (std::uint32_t id : order) {
        const Json& record = *by_id[id];
        const std::string key = id == *root ? std::string() : key_of(id);
        const std::string label = std::string(path) + ": entity " + key_of(id);
        if (const Json* prefab = record.find("prefab")) {
            std::string sub_error;
            auto sub = get(prefab->as_string(), &sub_error);
            if (!sub) return fail(sub_error);
            const Place place = [&](const std::string& t) {
                return Json(t.empty() ? key : key.empty() ? t : key + "/" + t);
            };
            for (PlannedPart& p : plan_instance(types_, *sub, &record, place, file_map, &tpl->warnings, label)) {
                tpl->parts.push_back({place(p.path).as_string(), std::move(p.components)});
            }
            tpl->sources.insert(tpl->sources.end(), sub->sources.begin(), sub->sources.end());
            tpl->warnings.insert(tpl->warnings.end(), sub->warnings.begin(), sub->warnings.end());
        } else {
            PrefabTemplate::Part part{key, Json::object()};
            for (const auto& [name, value] : record["components"].as_object()) {
                const TypeRegistry::Entry* t = types_.find(name);
                if (!t) {
                    tpl->warnings.push_back(label + ": " + name + ": unknown component (not registered)");
                    continue;
                }
                part.components[name] = remap(*t, value, file_map, &tpl->warnings, label + ": " + name);
            }
            tpl->parts.push_back(std::move(part));
        }
    }
    std::stable_partition(tpl->parts.begin(), tpl->parts.end(), [](const auto& p) { return p.path.empty(); });
    std::unordered_set<std::string> seen;
    for (const auto& p : tpl->parts) {
        if (!seen.insert(p.path).second) {
            return fail("path \"" + p.path + "\" is used twice (a record id collides with a part of the base prefab)");
        }
    }
    tpl->sources.emplace_back(std::string(path), src.version);
    return tpl;
}

} // namespace astral
