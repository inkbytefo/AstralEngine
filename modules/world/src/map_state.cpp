#include <astral/world/map_state.hpp>
#include <astral/world/transform.hpp>

#include <algorithm>
#include <unordered_map>

namespace astral {

namespace {

Json scene_of(const Json& record) {
    Json doc = Json::object();
    doc["format"] = "astral.scene";
    doc["version"] = 1;
    Json list = Json::array();
    list.push_back(record);
    doc["entities"] = std::move(list);
    return doc;
}

/// MapEntity::id → live entity, for the members of one cell.
std::unordered_map<std::uint64_t, Entity> members_of(World& world, CellKey cell) {
    std::unordered_map<std::uint64_t, Entity> out;
    world.each<const CellMember>([&](Entity e, const CellMember& m) {
        if (m.cell == cell) out.emplace(m.id, e);
    });
    return out;
}

Json save_one(World& world, PrefabLibrary& prefabs, Entity e) {
    Json doc = save_entities(world, prefabs, std::span(&e, 1));
    return doc["entities"].size() == 1 ? doc["entities"][0] : Json();
}

void destroy_with_parts(World& world, PrefabLibrary& prefabs, const std::vector<Entity>& roots) {
    for (Entity e : roots) {
        if (world.alive(e)) world.destroy(e);
    }
    sync_prefabs(world, prefabs); // removes the parts of destroyed instances
}

} // namespace

MapLayers::MapLayers(const MapDesc& map) : layers_(map.layers) {}

bool MapLayers::active(std::string_view layer) const noexcept {
    for (const LayerDesc& l : layers_) {
        if (l.name == layer) return l.active;
    }
    return false;
}

bool MapLayers::set(std::string_view layer, bool active) {
    for (LayerDesc& l : layers_) {
        if (l.name != layer) continue;
        if (l.active != active) {
            l.active = active;
            ++version_;
        }
        return true;
    }
    return false;
}

bool MapLayers::visible(const MapEntity& entity) const noexcept {
    if (entity.layers.empty()) return true;
    return std::any_of(entity.layers.begin(), entity.layers.end(), [&](const std::string& l) { return active(l); });
}

MapState::Loaded* MapState::find_loaded(CellKey cell) noexcept {
    for (Loaded& l : loaded_) {
        if (l.key == cell) return &l;
    }
    return nullptr;
}

const CellDiff* MapState::diff(CellKey cell) const noexcept {
    for (const Remembered& r : remembered_) {
        if (r.key == cell) return &r.diff;
    }
    return nullptr;
}

Json MapState::save_world_record(World& world, PrefabLibrary& prefabs, Entity e) const {
    Json record = save_one(world, prefabs, e);
    const Transform* t = world.get<const Transform>(e);
    if (record.is_object() && t && !world.has<Parent>(e)) {
        Json position = Json::array();
        position.push_back(origin_.x + static_cast<double>(t->position.x));
        position.push_back(origin_.y + static_cast<double>(t->position.y));
        position.push_back(origin_.z + static_cast<double>(t->position.z));
        record["components"]["astral::Transform"]["position"] = std::move(position);
    }
    return record;
}

LoadResult MapState::spawn(World& world, PrefabLibrary& prefabs, Loaded& cell, std::size_t index) {
    const MapEntity& e = cell.entities[index];
    LoadResult r = load_scene(world, prefabs, scene_of(e.record));
    if (!r.entities.empty()) {
        const Entity root = r.entities.front();
        if (Transform* t = world.get<Transform>(root); t && !world.has<Parent>(root)) {
            const Json& p = e.record["components"]["astral::Transform"]["position"];
            const double x = p.size() == 3 ? p[0].as_number() : static_cast<double>(t->position.x);
            const double y = p.size() == 3 ? p[1].as_number() : static_cast<double>(t->position.y);
            const double z = p.size() == 3 ? p[2].as_number() : static_cast<double>(t->position.z);
            t->position = {static_cast<float>(x - origin_.x), static_cast<float>(y - origin_.y), static_cast<float>(z - origin_.z)};
        }
        world.add(root, CellMember{cell.key, e.id});
        cell.baseline[index] = save_world_record(world, prefabs, root);
        cell.spawned[index] = true;
    }
    return r;
}

LoadResult MapState::spawn_cell(World& world, PrefabLibrary& prefabs, CellKey cell, std::span<const MapEntity> entities,
                                const MapLayers& layers) {
    if (Loaded* existing = find_loaded(cell)) despawn_cell(world, prefabs, existing->key);
    Loaded l{cell, {}, {}, {}};
    const CellDiff* d = diff(cell);
    for (const MapEntity& e : entities) {
        if (d && std::find(d->removed.begin(), d->removed.end(), e.id) != d->removed.end()) continue;
        MapEntity copy = e;
        if (d) {
            for (const Json& record : d->changed) {
                if (static_cast<std::uint64_t>(record["id"].as_number()) == e.id) copy.record = record;
            }
        }
        l.entities.push_back(std::move(copy));
    }
    l.baseline.resize(l.entities.size());
    l.spawned.assign(l.entities.size(), false);
    loaded_.push_back(std::move(l));
    Loaded& added = loaded_.back();

    LoadResult result;
    for (std::size_t i = 0; i < added.entities.size(); ++i) {
        if (!layers.visible(added.entities[i])) continue;
        LoadResult r = spawn(world, prefabs, added, i);
        result.entities.insert(result.entities.end(), r.entities.begin(), r.entities.end());
        result.errors.insert(result.errors.end(), r.errors.begin(), r.errors.end());
    }
    return result;
}

void MapState::despawn_cell(World& world, PrefabLibrary& prefabs, CellKey cell) {
    Loaded* l = find_loaded(cell);
    if (!l) return;
    const auto live = members_of(world, cell);

    CellDiff fresh;
    for (std::size_t i = 0; i < l->entities.size(); ++i) {
        if (!l->spawned[i]) continue;
        const std::uint64_t id = l->entities[i].id;
        auto it = live.find(id);
        if (it == live.end()) {
            fresh.removed.push_back(id);
            continue;
        }
        Json now = save_world_record(world, prefabs, it->second);
        if (now != l->baseline[i]) {
            now["id"] = id;
            fresh.changed.push_back(std::move(now));
        }
    }
    if (!fresh.removed.empty() || !fresh.changed.empty()) {
        Remembered* r = nullptr;
        for (Remembered& candidate : remembered_) {
            if (candidate.key == cell) r = &candidate;
        }
        if (!r) r = &remembered_.emplace_back(Remembered{cell, {}});
        r->diff.removed.insert(r->diff.removed.end(), fresh.removed.begin(), fresh.removed.end());
        for (Json& record : fresh.changed) {
            const double id = record["id"].as_number();
            auto same = std::find_if(r->diff.changed.begin(), r->diff.changed.end(),
                                     [&](const Json& j) { return j["id"].as_number() == id; });
            if (same != r->diff.changed.end()) *same = std::move(record);
            else r->diff.changed.push_back(std::move(record));
        }
    }

    std::vector<Entity> roots;
    roots.reserve(live.size());
    for (const auto& [id, e] : live) roots.push_back(e);
    destroy_with_parts(world, prefabs, roots);
    loaded_.erase(loaded_.begin() + (l - loaded_.data()));
}

void MapState::refresh_layers(World& world, PrefabLibrary& prefabs, const MapLayers& layers) {
    for (Loaded& l : loaded_) {
        std::vector<Entity> hide;
        const auto live = members_of(world, l.key);
        for (std::size_t i = 0; i < l.entities.size(); ++i) {
            const bool visible = layers.visible(l.entities[i]);
            if (visible && !l.spawned[i]) {
                spawn(world, prefabs, l, i);
            } else if (!visible && l.spawned[i]) {
                if (auto it = live.find(l.entities[i].id); it != live.end()) hide.push_back(it->second);
                l.spawned[i] = false;
            }
        }
        destroy_with_parts(world, prefabs, hide);
    }
}

} // namespace astral
