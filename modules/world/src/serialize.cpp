#include <astral/world/level_instance.hpp>
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

bool TypeRegistry::prepare_schema(const Entry& entry, const Json& input, Json& out, const LoadContext& ctx) {
    if (!input.is_object()) {
        out = input;
        return true;
    }
    std::uint32_t doc_ver = 1;
    if (const Json* sv = input.find("$schema_version")) {
        const double num = sv->as_number(-1.0);
        if (!sv->is_number() || num < 1.0 || !detail::number_fits<std::uint32_t>(num)) {
            ctx.error("invalid $schema_version " + sv->dump());
            return false;
        }
        doc_ver = static_cast<std::uint32_t>(num);
    }
    if (doc_ver > entry.schema_version) {
        ctx.error("schema version " + std::to_string(doc_ver) + " is newer than supported version " +
                  std::to_string(entry.schema_version));
        return false;
    }
    out = input;
    if (doc_ver < entry.schema_version) {
        if (!entry.migrate) {
            ctx.error("missing migration from schema version " + std::to_string(doc_ver) + " to " +
                      std::to_string(entry.schema_version));
            return false;
        }
        std::string err;
        if (!entry.migrate(doc_ver, out, err)) {
            ctx.error("migration v" + std::to_string(doc_ver) + "->v" + std::to_string(entry.schema_version) +
                      " failed: " + (err.empty() ? "unknown error" : err));
            return false;
        }
    }
    return true;
}

Json TypeRegistry::stamp_schema(const Entry& entry, Json value) {
    if (entry.schema_version > 1 && value.is_object()) {
        value["$schema_version"] = static_cast<double>(entry.schema_version);
    }
    return value;
}

bool TypeRegistry::set_schema_version(std::string_view name, std::uint32_t version, Migration migrate) {
    if (version == 0) return false;
    for (Entry& e : entries_) {
        if (e.name != name) continue;
        e.schema_version = version;
        e.migrate = std::move(migrate);
        return true;
    }
    return false;
}

Json TypeRegistry::save_entry(const Entry& entry, World& world, Entity entity, const SaveContext& ctx) {
    return stamp_schema(entry, entry.save(world, entity, ctx));
}

void TypeRegistry::load_entry(const Entry& entry, World& world, Entity entity, const Json& value, const LoadContext& ctx) {
    Json prepared;
    if (!prepare_schema(entry, value, prepared, ctx)) return;
    entry.load(world, entity, prepared, ctx);
}

void register_world_types(TypeRegistry& types) {
    types.add<Name>();
    types.add<Transform>();
    types.add<Parent>();
    register_level_instance_types(types);
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
            if (t.has(world, saved[i])) components[t.name] = TypeRegistry::save_entry(t, world, saved[i], ctx);
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
            TypeRegistry::load_entry(*t, world, entities[i], value, c);
        }
    }
    result.entities = std::move(entities);
    return result;
}

namespace {
std::string live_ref(WorldId world_id, Entity e) {
    return "#w" + std::to_string(world_id) + ":" + std::to_string(e.index) + "." + std::to_string(e.generation);
}
} // namespace

SaveContext live_save_context(World& world) {
    SaveContext ctx;
    ctx.ref = [&world](Entity e) { return world.alive(e) ? Json(live_ref(world.id(), e)) : Json(nullptr); };
    return ctx;
}

LoadContext live_load_context(World& world, std::vector<std::string>* errors, std::string path) {
    LoadContext ctx;
    ctx.errors = errors;
    ctx.path = std::move(path);
    ctx.resolve = [&world](const Json& j, Entity& out) {
        if (!j.is_string()) return false;
        const std::string& s = j.as_string();
        if (s.size() < 4 || s[0] != '#') return false;
        std::size_t pos = 1;
        if (s[1] == 'w') {
            const std::size_t colon = s.find(':', 2);
            if (colon == std::string::npos || colon <= 2) return false;
            try {
                const WorldId ref_world = static_cast<WorldId>(std::stoull(s.substr(2, colon - 2)));
                if (ref_world != world.id()) return false;
            } catch (...) {
                return false;
            }
            pos = colon + 1;
        }
        const std::size_t dot = s.find('.', pos);
        if (dot == std::string::npos || dot <= pos) return false;
        try {
            out = Entity{static_cast<std::uint32_t>(std::stoul(s.substr(pos, dot - pos))),
                         static_cast<std::uint32_t>(std::stoul(s.substr(dot + 1)))};
        } catch (...) {
            return false;
        }
        return world.alive(out);
    };
    return ctx;
}

Json component_to_json(World& world, const TypeRegistry::Entry& type, Entity entity) {
    if (!world.alive(entity) || !type.has(world, entity)) return nullptr;
    return TypeRegistry::save_entry(type, world, entity, live_save_context(world));
}

std::vector<std::string> component_from_json(World& world, const TypeRegistry::Entry& type, Entity entity, const Json& value) {
    std::vector<std::string> errors;
    if (!world.alive(entity)) {
        errors.push_back("component_from_json: dead entity");
        return errors;
    }
    TypeRegistry::load_entry(type, world, entity, value, live_load_context(world, &errors, std::string(type.name)));
    return errors;
}

} // namespace astral

