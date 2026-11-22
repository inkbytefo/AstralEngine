#include <astral/world/serialize.hpp>
#include <astral/world/scene.hpp>

namespace astral {

namespace {
constexpr std::string_view format_name = "astral.world";
constexpr int format_version = 1;
} // namespace

const TypeRegistry::Entry* TypeRegistry::find(std::string_view name) const noexcept {
    for (const Entry& e : entries_) {
        if (e.name == name) return &e;
    }
    return nullptr;
}

void register_world_types(TypeRegistry& types) {
    types.add<Name>();
    types.add<Transform>();
    types.add<Parent>();
}

Json save_world(World& world, const TypeRegistry& types) {
    // Pass 1: which entities are saved, and their file ids (references need them all first).
    std::vector<Entity> saved;
    std::unordered_map<std::uint64_t, std::uint32_t> ids;
    const auto key = [](Entity e) { return (std::uint64_t{e.generation} << 32) | e.index; };
    world.for_each_entity([&](Entity e) {
        if (types.is_excluded(world, e)) return;
        for (const auto& t : types.entries()) {
            if (t.has(world, e)) {
                ids.emplace(key(e), static_cast<std::uint32_t>(saved.size()));
                saved.push_back(e);
                return;
            }
        }
    });
    SaveContext ctx;
    ctx.ref = [&](Entity e) {
        auto it = ids.find(key(e));
        return it == ids.end() ? Json(nullptr) : Json(it->second); // not saved: null
    };
    Json doc = Json::object();
    doc["format"] = format_name;
    doc["version"] = format_version;
    Json& list = doc["entities"];
    list = Json::array();
    for (std::size_t i = 0; i < saved.size(); ++i) {
        Json record = Json::object();
        record["id"] = static_cast<std::uint32_t>(i);
        Json& components = record["components"];
        components = Json::object();
        for (const auto& t : types.entries()) { // registration order: stable output
            if (t.has(world, saved[i])) components[t.name] = t.save(world, saved[i], ctx);
        }
        list.push_back(std::move(record));
    }
    return doc;
}

LoadResult load_world(World& world, const TypeRegistry& types, const Json& document) {
    LoadResult result;
    if (document["format"].as_string() != format_name) {
        result.errors.push_back("not an astral.world document");
        return result;
    }
    if (document["version"].as_number(-1) != format_version) {
        result.errors.push_back("unsupported astral.world version " + document["version"].dump());
        return result;
    }
    const Json& list = document["entities"];
    // Entities first, so references between them resolve in any order. Document ids are the
    // record positions; the explicit "id" is informative.
    std::vector<Entity> entities;
    for (std::size_t i = 0; i < list.size(); ++i) entities.push_back(world.create());
    LoadContext ctx;
    ctx.errors = &result.errors;
    ctx.resolve = [&](const Json& j, Entity& out) {
        const double id = j.as_number(-1);
        if (!j.is_number() || id < 0 || id >= static_cast<double>(entities.size()) ||
            id != static_cast<double>(static_cast<std::size_t>(id))) {
            return false;
        }
        out = entities[static_cast<std::size_t>(id)];
        return true;
    };
    for (std::size_t i = 0; i < list.size(); ++i) {
        const Json& components = list[i]["components"];
        for (const auto& [name, value] : components.as_object()) {
            LoadContext c = ctx;
            c.path = "entity " + std::to_string(i) + ": " + name;
            const TypeRegistry::Entry* t = types.find(name);
            if (!t) {
                c.error("unknown component (not registered)");
                continue;
            }
            t->load(world, entities[i], value, c);
        }
    }
    result.entities = std::move(entities);
    return result;
}

namespace {
std::string live_ref(Entity e) { return "#" + std::to_string(e.index) + "." + std::to_string(e.generation); }
} // namespace

SaveContext live_save_context(World& world) {
    SaveContext ctx;
    ctx.ref = [&world](Entity e) { return world.alive(e) ? Json(live_ref(e)) : Json(nullptr); };
    return ctx;
}

LoadContext live_load_context(World& world, std::vector<std::string>* errors, std::string path) {
    LoadContext ctx;
    ctx.errors = errors;
    ctx.path = std::move(path);
    ctx.resolve = [&world](const Json& j, Entity& out) {
        const std::string& s = j.as_string();
        const std::size_t dot = s.find('.');
        if (!j.is_string() || s.size() < 4 || s[0] != '#' || dot == std::string::npos) return false;
        try {
            out = Entity{static_cast<std::uint32_t>(std::stoul(s.substr(1, dot - 1))), static_cast<std::uint32_t>(std::stoul(s.substr(dot + 1)))};
        } catch (...) {
            return false;
        }
        return world.alive(out);
    };
    return ctx;
}

Json component_to_json(World& world, const TypeRegistry::Entry& type, Entity entity) {
    if (!world.alive(entity) || !type.has(world, entity)) return nullptr;
    return type.save(world, entity, live_save_context(world));
}

std::vector<std::string> component_from_json(World& world, const TypeRegistry::Entry& type, Entity entity, const Json& value) {
    std::vector<std::string> errors;
    if (!world.alive(entity)) {
        errors.push_back("component_from_json: dead entity");
        return errors;
    }
    type.load(world, entity, value, live_load_context(world, &errors, std::string(type.name)));
    return errors;
}

} // namespace astral

