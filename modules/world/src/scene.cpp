#include <astral/world/scene.hpp>
#include <astral/world/transform.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>

namespace astral {

namespace {

constexpr std::string_view scene_format = "astral.scene";
constexpr int scene_version = 1;
/// Generation of the stand-in entities remap() hands out while rewriting references.
constexpr std::uint32_t placeholder_generation = 0xFFFFFFFFu;
constexpr std::string_view parent_name = Reflect<Parent>::name;

/// Internal: on an instance whose parts were spawned, the template they came from. `from` is null
/// (and `failed_path` set) when the prefab could not be built: the error was reported once.
struct PrefabSpawned {
    std::shared_ptr<const PrefabTemplate> from;
    std::string failed_path;
};

void warn(std::vector<std::string>* errors, std::string message) {
    if (errors) errors->push_back(std::move(message));
}

std::uint64_t entity_key(Entity e) { return (std::uint64_t{e.generation} << 32) | e.index; }

/// Same rules as asset ids: case and separators ignored.
std::string normalize(std::string_view path) {
    std::string out;
    out.reserve(path.size());
    for (char c : path) out += c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    while (out.starts_with("./")) out.erase(0, 2);
    return out;
}

std::string key_of(std::uint32_t id) { return std::to_string(id); }

std::optional<std::uint32_t> record_id(const Json& j) {
    if (!j.is_number()) return std::nullopt;
    const double d = j.as_number();
    if (d < 0 || d > 4294967295.0 || d != std::floor(d)) return std::nullopt;
    return static_cast<std::uint32_t>(d);
}

/// A reference written in a document: record id, and a path inside that record's instance.
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

/// Canonical reference in a scene: a number for a record, "id/path" for a part of an instance.
Json placed_ref(std::uint32_t id, const std::string& rest) {
    return rest.empty() ? Json(id) : Json(key_of(id) + "/" + rest);
}

/// Lookup key of a canonical reference.
std::string ref_key(const Json& j) {
    if (auto id = record_id(j)) return key_of(*id);
    return j.is_string() ? j.as_string() : std::string();
}

/// Reference to an entity that is in no document (runtime respawns): "#index.generation".
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

// --- merge patches ------------------------------------------------------------------------------

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

// --- references ---------------------------------------------------------------------------------

/// Reference in -> reference out; nullopt: names nothing.
using RefMap = std::function<std::optional<Json>(const Json&)>;

/// Reads `value` as the component type and writes it back with every entity reference mapped:
/// canonical numbers, every field present, references in the destination's terms.
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

// --- instances ----------------------------------------------------------------------------------

struct PlannedPart {
    std::string path; ///< Relative to the instance ("" = its root).
    Json components;  ///< References already placed.
};

using Place = std::function<Json(const std::string&)>; ///< Template path -> placed reference.

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

/// The parts of an instance of `tpl`: the template placed, then the record's overrides applied.
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

/// Creates entities first and loads components last, so references resolve in any order.
struct Spawner {
    World& world;
    const TypeRegistry& types;
    std::vector<std::string>* errors;
    std::unordered_map<std::string, Entity> refs;
    struct Pending {
        Entity entity;
        Json components;
        bool keep_existing; ///< Instance roots: components the entity has win.
        std::string label;
    };
    std::vector<Pending> pending;
    std::unordered_set<const PrefabTemplate*> warned;

    bool resolve(const Json& j, Entity& out) const {
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

    void finish() {
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

    /// Plans and creates the parts of `root` as an instance of `tpl`, placed at `root_ref`.
    void spawn(Entity root, const std::shared_ptr<const PrefabTemplate>& tpl, const Json* record, const Json& root_ref,
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
};

std::vector<Entity> parts_of(World& world, Entity instance) {
    std::vector<Entity> out;
    world.each<const PrefabPart>([&](Entity e, const PrefabPart& p) {
        if (p.instance == instance) out.push_back(e);
    });
    return out;
}

/// What an instance changed relative to `tpl`, as record members (components, removed, parts,
/// deleted; empty ones left out). `outer` writes references to entities outside the instance.
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

bool check_format(const Json& doc, std::string* error) {
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

} // namespace

// --- PrefabLibrary ------------------------------------------------------------------------------

PrefabLibrary::PrefabLibrary(const TypeRegistry& types, DocumentSource source) : types_(types), source_(std::move(source)) {}

std::shared_ptr<const PrefabTemplate> PrefabLibrary::get(std::string_view path, std::string* error) {
    const std::string key = normalize(path);
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
    if (!check_format(doc, &format_error)) return fail(format_error);

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

// --- instances ----------------------------------------------------------------------------------

LoadResult instantiate_prefab(World& world, PrefabLibrary& prefabs, Entity instance) {
    LoadResult result;
    const PrefabInstance* pi = world.get<const PrefabInstance>(instance);
    if (!pi) {
        result.errors.push_back("instantiate_prefab: the entity has no PrefabInstance");
        return result;
    }
    const std::string path = pi->path;
    std::string error;
    auto tpl = prefabs.get(path, &error);
    if (!tpl) {
        result.errors.push_back(error);
        world.add(instance, PrefabSpawned{nullptr, path});
        return result;
    }
    for (Entity e : parts_of(world, instance)) world.destroy(e);
    Spawner s{world, prefabs.types(), &result.errors, {}, {}, {}};
    s.spawn(instance, tpl, nullptr, Json(raw_key(instance)), {}, path);
    s.finish();
    result.entities.push_back(instance);
    for (Entity e : parts_of(world, instance)) result.entities.push_back(e);
    return result;
}

void sync_prefabs(World& world, PrefabLibrary& prefabs, std::vector<std::string>* errors) {
    const TypeRegistry& types = prefabs.types();

    // Parts of destroyed / unlinked instances.
    std::vector<Entity> stale;
    world.each<const PrefabPart>([&](Entity e, const PrefabPart& p) {
        if (!world.alive(p.instance) || !world.has<PrefabInstance>(p.instance) || !world.has<PrefabSpawned>(p.instance)) {
            stale.push_back(e);
        }
    });
    for (Entity e : stale) world.destroy(e);
    std::vector<Entity> unlinked;
    world.query<const PrefabSpawned>().without<PrefabInstance>().each([&](Entity e, const PrefabSpawned&) { unlinked.push_back(e); });
    for (Entity e : unlinked) world.remove<PrefabSpawned>(e);

    struct Todo {
        Entity entity;
        std::shared_ptr<const PrefabTemplate> tpl;
        std::shared_ptr<const PrefabTemplate> old;
    };
    std::vector<Todo> todo;
    std::vector<std::pair<Entity, std::string>> failed;
    world.each<const PrefabInstance>([&](Entity e, const PrefabInstance& pi) {
        const PrefabSpawned* s = world.get<const PrefabSpawned>(e);
        std::string error;
        auto tpl = prefabs.get(pi.path, &error);
        if (!tpl) {
            if (!s || s->failed_path != pi.path) {
                warn(errors, error);
                failed.emplace_back(e, pi.path);
            }
            return; // a spawned instance keeps its last good parts
        }
        if (s && s->from == tpl) return;
        todo.push_back({e, tpl, s ? s->from : nullptr});
    });
    for (auto& [e, path] : failed) {
        PrefabSpawned* s = world.get<PrefabSpawned>(e);
        if (s) s->failed_path = path;
        else world.add(e, PrefabSpawned{nullptr, path});
    }
    if (todo.empty()) return;

    Spawner s{world, types, errors, {}, {}, {}};
    const RefMap same = [](const Json& r) -> std::optional<Json> { return r; };
    const std::function<Json(Entity)> raw = [](Entity e) { return Json(raw_key(e)); };
    std::vector<std::pair<Entity, Json>> records;
    records.reserve(todo.size());
    for (const Todo& t : todo) {
        const Json root_ref(raw_key(t.entity));
        Json record = Json::object();
        if (t.old) {
            // Respawn: keep what this instance changed, drop what the old prefab gave it.
            record = compute_overrides(world, types, t.entity, *t.old, root_ref, raw);
            if (normalize(t.old->path) != normalize(t.tpl->path)) { // another prefab: only root overrides apply
                Json root_only = Json::object();
                if (const Json* c = record.find("components")) root_only["components"] = *c;
                record = std::move(root_only);
            }
            for (const auto& [name, value] : t.old->parts[0].components.as_object()) {
                if (const TypeRegistry::Entry* entry = types.find(name); entry && entry->has(world, t.entity)) {
                    entry->remove(world, t.entity);
                }
            }
            for (Entity e : parts_of(world, t.entity)) world.destroy(e);
        }
        records.emplace_back(t.entity, std::move(record));
    }
    for (std::size_t i = 0; i < todo.size(); ++i) {
        const bool respawn = todo[i].old != nullptr;
        s.spawn(todo[i].entity, todo[i].tpl, respawn ? &records[i].second : nullptr, Json(raw_key(todo[i].entity)), same,
                todo[i].tpl->path);
    }
    s.finish();
}

// --- scenes -------------------------------------------------------------------------------------

LoadResult load_scene(World& world, PrefabLibrary& prefabs, const Json& document) {
    LoadResult result;
    std::string error;
    if (!check_format(document, &error)) {
        result.errors.push_back(error);
        return result;
    }
    Spawner s{world, prefabs.types(), &result.errors, {}, {}, {}};
    struct Record {
        const Json* json;
        std::uint32_t id;
        Entity entity;
    };
    std::vector<Record> records;
    std::unordered_set<std::uint32_t> seen;
    const Json& list = document["entities"];
    for (std::size_t i = 0; i < list.size(); ++i) {
        const auto id = record_id(list[i]["id"]);
        if (!id) {
            result.errors.push_back("record " + std::to_string(i) + ": missing or invalid \"id\"");
            continue;
        }
        if (!seen.insert(*id).second) {
            result.errors.push_back("record " + std::to_string(i) + ": duplicate id " + key_of(*id));
            continue;
        }
        const Entity e = world.create(SceneId{*id});
        s.refs[key_of(*id)] = e;
        records.push_back({&list[i], *id, e});
        result.entities.push_back(e);
    }
    const RefMap file_map = [](const Json& r) -> std::optional<Json> {
        const auto ref = parse_ref(r);
        if (!ref) return std::nullopt;
        return placed_ref(ref->first, ref->second);
    };
    for (const Record& r : records) {
        const std::string label = "entity " + key_of(r.id);
        const Json* prefab = r.json->find("prefab");
        if (!prefab) {
            s.pending.push_back({r.entity, (*r.json)["components"], false, label});
            continue;
        }
        const std::string path = prefab->as_string();
        world.add(r.entity, PrefabInstance{path});
        std::string prefab_error;
        auto tpl = prefabs.get(path, &prefab_error);
        if (!tpl) { // keep the record's own values; the instance spawns once the prefab loads
            result.errors.push_back(label + ": " + prefab_error);
            s.pending.push_back({r.entity, (*r.json)["components"], false, label});
            world.add(r.entity, PrefabSpawned{nullptr, path});
            continue;
        }
        s.spawn(r.entity, tpl, r.json, Json(r.id), file_map, label);
    }
    s.finish();
    return result;
}

namespace {

Json save_selected(World& world, PrefabLibrary& prefabs, std::vector<Entity> saved);

} // namespace

Json save_scene(World& world, PrefabLibrary& prefabs) {
    const TypeRegistry& types = prefabs.types();
    std::vector<Entity> saved;
    world.for_each_entity([&](Entity e) {
        if (world.has<PrefabPart>(e) || types.is_excluded(world, e)) return;
        bool any = world.has<PrefabInstance>(e);
        for (const auto& t : types.entries()) any = any || t.has(world, e);
        if (any) saved.push_back(e);
    });
    return save_selected(world, prefabs, std::move(saved));
}

Json save_entities(World& world, PrefabLibrary& prefabs, std::span<const Entity> entities) {
    std::vector<Entity> saved;
    for (Entity e : entities) {
        if (world.alive(e) && !world.has<PrefabPart>(e)) saved.push_back(e);
    }
    return save_selected(world, prefabs, std::move(saved));
}

namespace {

Json save_selected(World& world, PrefabLibrary& prefabs, std::vector<Entity> saved) {
    const TypeRegistry& types = prefabs.types();

    // Ids: keep existing ones (the first holder of a duplicate keeps it), number the rest after them.
    std::unordered_set<std::uint32_t> used;
    std::vector<Entity> unnumbered;
    std::uint32_t next = 0;
    for (Entity e : saved) {
        const SceneId* id = world.get<const SceneId>(e);
        if (id && used.insert(id->value).second) next = std::max(next, id->value);
        else unnumbered.push_back(e);
    }
    for (Entity e : unnumbered) world.add(e, SceneId{++next});
    std::unordered_map<std::uint64_t, std::uint32_t> id_of;
    for (Entity e : saved) id_of.emplace(entity_key(e), world.get<const SceneId>(e)->value);
    std::sort(saved.begin(), saved.end(), [&](Entity a, Entity b) { return id_of[entity_key(a)] < id_of[entity_key(b)]; });

    const std::function<Json(Entity)> outer = [&](Entity e) -> Json {
        if (auto it = id_of.find(entity_key(e)); it != id_of.end()) return Json(it->second);
        if (const PrefabPart* p = world.get<const PrefabPart>(e)) {
            if (auto it = id_of.find(entity_key(p->instance)); it != id_of.end()) return placed_ref(it->second, p->path);
        }
        return nullptr; // not in the scene
    };
    SaveContext ctx;
    ctx.ref = outer;

    Json doc = Json::object();
    doc["format"] = scene_format;
    doc["version"] = scene_version;
    Json& list = doc["entities"];
    list = Json::array();
    for (Entity e : saved) {
        const std::uint32_t id = id_of[entity_key(e)];
        Json record = Json::object();
        record["id"] = id;
        const PrefabInstance* pi = world.get<const PrefabInstance>(e);
        const PrefabSpawned* spawned = world.get<const PrefabSpawned>(e);
        if (pi) record["prefab"] = pi->path;
        if (pi && spawned && spawned->from && normalize(spawned->from->path) == normalize(pi->path)) {
            Json overrides = compute_overrides(world, types, e, *spawned->from, Json(id), outer);
            for (const auto& [key, value] : overrides.as_object()) record[key] = value;
        } else { // a plain entity, or an instance not spawned yet: its own components in full
            Json components = Json::object();
            for (const auto& t : types.entries()) {
                if (t.has(world, e)) components[t.name] = t.save(world, e, ctx);
            }
            if (!pi || components.size()) record["components"] = std::move(components);
        }
        list.push_back(std::move(record));
    }
    return doc;
}

} // namespace

Entity find_by_name(World& world, std::string_view name) {
    Entity found{};
    world.each<const Name>([&](Entity e, const Name& n) {
        if (!found.valid() && n.value == name) found = e;
    });
    return found;
}

Entity find_part(World& world, Entity instance, std::string_view name) {
    Entity found{};
    world.each<const PrefabPart, const Name>([&](Entity e, const PrefabPart& p, const Name& n) {
        if (!found.valid() && p.instance == instance && n.value == name) found = e;
    });
    return found;
}

} // namespace astral
