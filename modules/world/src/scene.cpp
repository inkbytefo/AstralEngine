#include "scene_internal.hpp"

#include <algorithm>

namespace astral {

using namespace scene_detail;

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
            if (normalize_path(t.old->path) != normalize_path(t.tpl->path)) { // another prefab: only root overrides apply
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
    if (!check_scene_format(document, &error)) {
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
        if (pi && spawned && spawned->from && normalize_path(spawned->from->path) == normalize_path(pi->path)) {
            Json overrides = compute_overrides(world, types, e, *spawned->from, Json(id), outer);
            for (const auto& [key, value] : overrides.as_object()) record[key] = value;
        } else { // a plain entity, or an instance not spawned yet: its own components in full
            Json components = Json::object();
            for (const auto& t : types.entries()) {
                if (t.has(world, e)) components[t.name] = TypeRegistry::save_entry(t, world, e, ctx);
            }
            if (!pi || components.size()) record["components"] = std::move(components);
        }
        list.push_back(std::move(record));
    }
    return doc;
}

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
