// The `astral` table: entity, component proxy, system, event, timer, and log bindings.
#include "bindings_internal.hpp"

#include <astral/world/scene.hpp>

#include <algorithm>
#include <cstring>

namespace astral {

namespace bindings_detail {

namespace {

std::string known_list(Impl& impl) {
    std::string out;
    for (const auto& e : impl.env.types.entries()) out += (out.empty() ? "" : ", ") + std::string(e.name);
    return out;
}

} // namespace

std::uint32_t require_type(lua_State* L, Impl& impl, std::string_view name) {
    const int i = impl.type_index(name);
    if (i < 0) {
        luaL_error(L, "unknown component \"%s\" (register it in C++ with types.add<T>() or define it with astral.define_component; known: %s)",
                   std::string(name).c_str(), known_list(impl).c_str());
    }
    return static_cast<std::uint32_t>(i);
}

Entity check_entity(lua_State* L, int index) {
    Entity e;
    if (!read_entity(L, index, e) || !e.valid()) luaL_typeerror(L, index, "entity");
    return e;
}

Quat check_quat(lua_State* L, int index) {
    float v[4];
    if (!read_floats_checked(L, index, v)) luaL_typeerror(L, index, "quaternion {x, y, z, w}");
    return {v[0], v[1], v[2], v[3]};
}

void push_quat(lua_State* L, Quat q) {
    lua_createtable(L, 0, 4);
    const double values[4] = {q.x, q.y, q.z, q.w};
    constexpr const char* names[] = {"x", "y", "z", "w"};
    for (int i = 0; i < 4; ++i) {
        lua_pushnumber(L, values[i]);
        lua_setfield(L, -2, names[i]);
    }
}

void set_function(lua_State* L, const char* name, lua_CFunction fn) {
    lua_pushcfunction(L, fn, name);
    lua_setfield(L, -2, name);
}

} // namespace bindings_detail

namespace {

using namespace bindings_detail;

void push_component(lua_State* L, Entity e, std::uint32_t type) {
    auto* box = static_cast<ComponentBox*>(lua_newuserdatataggedwithmetatable(L, sizeof(ComponentBox), tag_component));
    box->entity = e;
    box->type = type;
}

// --- component proxies: proxy.field reads and writes the live component ------------------------

const FieldInfo& require_field(lua_State* L, const TypeRegistry::Entry& entry, std::string_view key) {
    const FieldInfo* f = find_field(entry.fields, key);
    if (!f) {
        std::string names;
        for (const FieldInfo& x : entry.fields) names += (names.empty() ? "" : ", ") + x.name;
        luaL_error(L, "%s has no field \"%s\" (fields: %s)", std::string(entry.name).c_str(), std::string(key).c_str(), names.c_str());
    }
    return *f;
}

int component_index(lua_State* L) {
    Impl& impl = Impl::of(L);
    auto* box = static_cast<ComponentBox*>(lua_touserdatatagged(L, 1, tag_component));
    const TypeRegistry::Entry& entry = entry_of(impl, box->type);
    const FieldInfo& f = require_field(L, entry, luaL_checkstring(L, 2));
    const std::byte* base = static_cast<const World&>(impl.env.world).get_raw(box->entity, entry.component);
    if (!base) luaL_error(L, "%s: the entity no longer has this component", std::string(entry.name).c_str());
    push_field(L, f, base, impl.env.world);
    return 1;
}

int component_newindex(lua_State* L) {
    Impl& impl = Impl::of(L);
    auto* box = static_cast<ComponentBox*>(lua_touserdatatagged(L, 1, tag_component));
    const TypeRegistry::Entry& entry = entry_of(impl, box->type);
    const FieldInfo& f = require_field(L, entry, luaL_checkstring(L, 2));
    std::byte* base = impl.env.world.get_raw(box->entity, entry.component); // marks the component changed
    if (!base) luaL_error(L, "%s: the entity no longer has this component", std::string(entry.name).c_str());
    std::string error;
    if (!read_field(L, 3, f, base, &error)) luaL_error(L, "%s: %s", std::string(entry.name).c_str(), error.c_str());
    return 0;
}

int component_tostring(lua_State* L) {
    Impl& impl = Impl::of(L);
    auto* box = static_cast<ComponentBox*>(lua_touserdatatagged(L, 1, tag_component));
    lua_pushfstring(L, "%s of entity#%d", std::string(entry_of(impl, box->type).name).c_str(), static_cast<int>(box->entity.index));
    return 1;
}

// --- entities ---------------------------------------------------------------------------------

Entity check_alive(lua_State* L, Impl& impl, int index, const char* what) {
    const Entity e = check_entity(L, index);
    if (!impl.env.world.alive(e)) luaL_error(L, "%s: the entity is dead (check entity:alive() when you keep one across frames)", what);
    return e;
}

int entity_get(lua_State* L) {
    Impl& impl = Impl::of(L);
    const Entity e = check_alive(L, impl, 1, "get");
    const std::uint32_t type = require_type(L, impl, luaL_checkstring(L, 2));
    if (impl.env.world.has_raw(e, entry_of(impl, type).component)) push_component(L, e, type);
    else lua_pushnil(L);
    return 1;
}

int entity_has(lua_State* L) {
    Impl& impl = Impl::of(L);
    const Entity e = check_alive(L, impl, 1, "has");
    const std::uint32_t type = require_type(L, impl, luaL_checkstring(L, 2));
    lua_pushboolean(L, impl.env.world.has_raw(e, entry_of(impl, type).component));
    return 1;
}

void add_component(lua_State* L, Impl& impl, Entity e, std::uint32_t type, int value_index) {
    const TypeRegistry::Entry& entry = entry_of(impl, type);
    const Json json = lua_isnoneornil(L, value_index) ? Json::object() : lua_to_json(L, value_index, &entry.fields);
    std::vector<std::string> errors;
    entry.load(impl.env.world, e, json, live_load_context(impl.env.world, &errors, std::string(entry.name)));
    if (!errors.empty()) luaL_error(L, "%s", errors.front().c_str());
}

int entity_add(lua_State* L) {
    Impl& impl = Impl::of(L);
    const Entity e = check_alive(L, impl, 1, "add");
    const std::uint32_t type = require_type(L, impl, luaL_checkstring(L, 2));
    add_component(L, impl, e, type, 3);
    push_component(L, e, type);
    return 1;
}

int entity_remove(lua_State* L) {
    Impl& impl = Impl::of(L);
    const Entity e = check_alive(L, impl, 1, "remove");
    const std::uint32_t type = require_type(L, impl, luaL_checkstring(L, 2));
    impl.env.world.remove_raw(e, entry_of(impl, type).component);
    return 0;
}

int entity_destroy(lua_State* L) {
    Impl& impl = Impl::of(L);
    const Entity e = check_entity(L, 1);
    if (impl.env.world.alive(e)) impl.env.world.destroy(e);
    return 0;
}

int entity_alive(lua_State* L) {
    Impl& impl = Impl::of(L);
    Entity e;
    lua_pushboolean(L, read_entity(L, 1, e) && impl.env.world.alive(e));
    return 1;
}

int entity_id(lua_State* L) {
    const Entity e = check_entity(L, 1);
    lua_pushnumber(L, static_cast<double>(e.generation) * 4294967296.0 + e.index);
    return 1;
}

int entity_eq(lua_State* L) {
    Entity a, b;
    lua_pushboolean(L, read_entity(L, 1, a) && read_entity(L, 2, b) && a == b);
    return 1;
}

int entity_tostring(lua_State* L) {
    const Entity e = check_entity(L, 1);
    lua_pushfstring(L, "entity#%d.%d", static_cast<int>(e.index), static_cast<int>(e.generation));
    return 1;
}

// --- registrations ----------------------------------------------------------------------------

Module& active_module(lua_State* L, const char* what) {
    Impl& impl = Impl::of(L);
    if (!impl.active) luaL_error(L, "%s can only be called while a script is loading or running", what);
    return *impl.active;
}

int function_ref(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TFUNCTION);
    return lua_ref(L, index);
}

ScriptPhase phase_from(lua_State* L, const char* name) {
    const std::string_view s = name;
    if (s == "PreUpdate") return ScriptPhase::PreUpdate;
    if (s == "Update") return ScriptPhase::Update;
    if (s == "FixedUpdate") return ScriptPhase::FixedUpdate;
    if (s == "PostUpdate") return ScriptPhase::PostUpdate;
    luaL_error(L, "unknown phase \"%s\" (PreUpdate, Update, FixedUpdate, PostUpdate)", name);
}

std::vector<std::string> string_list(lua_State* L, int table, const char* key) {
    std::vector<std::string> out;
    lua_getfield(L, table, key);
    if (lua_istable(L, -1)) {
        const int n = lua_objlen(L, -1);
        for (int i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            if (lua_type(L, -1) != LUA_TSTRING) luaL_error(L, "%s must be a list of component names", key);
            out.emplace_back(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    } else if (!lua_isnil(L, -1)) {
        luaL_error(L, "%s must be a list of component names", key);
    }
    lua_pop(L, 1);
    return out;
}

int astral_system(lua_State* L) {
    Impl& impl = Impl::of(L);
    Module& m = active_module(L, "astral.system");
    luaL_checktype(L, 1, LUA_TTABLE);
    SystemDef def;
    lua_getfield(L, 1, "name");
    def.name = lua_isstring(L, -1) ? lua_tostring(L, -1) : "system";
    lua_pop(L, 1);
    lua_getfield(L, 1, "phase");
    def.phase = phase_from(L, lua_isstring(L, -1) ? lua_tostring(L, -1) : "Update");
    lua_pop(L, 1);
    for (const std::string& name : string_list(L, 1, "query")) {
        const std::uint32_t t = require_type(L, impl, name);
        def.types.push_back(t);
        def.required.push_back(entry_of(impl, t).component);
    }
    for (const std::string& name : string_list(L, 1, "with")) def.required.push_back(entry_of(impl, require_type(L, impl, name)).component);
    for (const std::string& name : string_list(L, 1, "without")) def.excluded.push_back(entry_of(impl, require_type(L, impl, name)).component);
    // Validate everything first: a registry ref taken before a luaL_error would never be released.
    lua_getfield(L, 1, "update");
    const bool has_update = lua_isfunction(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "run");
    const bool has_run = lua_isfunction(L, -1);
    lua_pop(L, 1);
    if (has_update == has_run) luaL_error(L, "system \"%s\" needs exactly one of update (per entity) or run (once per phase)", def.name.c_str());
    if (has_update && def.types.empty() && def.required.empty()) luaL_error(L, "system \"%s\": update needs a query", def.name.c_str());
    lua_getfield(L, 1, has_update ? "update" : "run");
    (has_update ? def.update_ref : def.run_ref) = lua_ref(L, -1);
    lua_pop(L, 1);
    m.systems.push_back(std::move(def));
    return 0;
}

int astral_on(lua_State* L) {
    Impl& impl = Impl::of(L);
    Module& m = active_module(L, "astral.on");
    const std::string event = luaL_checkstring(L, 1);
    const EventRegistry::Entry* entry = impl.env.events.find(event);
    if (!entry) {
        luaL_error(L, "unknown event \"%s\" (register it in C++ with events.add<T>() or define it with astral.define_event)", event.c_str());
    }
    ListenerDef l;
    l.event = event;
    luaL_checktype(L, 2, LUA_TFUNCTION);
    l.phase = lua_isnoneornil(L, 3) ? ScriptPhase::Update : phase_from(L, luaL_checkstring(L, 3)); // may raise: before the ref
    l.fn_ref = function_ref(L, 2);
    entry->read(impl.env.world, l.cursor, [](const Json&) {}); // only events from now on
    m.listeners.push_back(std::move(l));
    return 0;
}

int astral_behaviour(lua_State* L) {
    Module& m = active_module(L, "astral.behaviour");
    luaL_checktype(L, 1, LUA_TTABLE);
    if (m.behaviour_ref) lua_unref(L, m.behaviour_ref);
    m.behaviour_ref = lua_ref(L, 1);
    lua_pushvalue(L, 1);
    return 1;
}

int astral_define_component(lua_State* L) {
    Impl& impl = Impl::of(L);
    const std::string name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<FieldDef> defs;
    lua_pushnil(L);
    while (lua_next(L, 2) != 0) {
        if (lua_type(L, -2) != LUA_TSTRING) luaL_error(L, "%s: field names must be strings", name.c_str());
        FieldDef d;
        d.name = lua_tostring(L, -2);
        std::string type_name;
        if (lua_type(L, -1) == LUA_TSTRING) {
            type_name = lua_tostring(L, -1);
        } else if (lua_istable(L, -1)) { // { type = "f32", default = 100 }
            lua_getfield(L, -1, "type");
            type_name = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
            lua_pop(L, 1);
            lua_getfield(L, -1, "default");
            d.default_value = lua_to_json(L, -1, nullptr);
            lua_pop(L, 1);
        }
        const auto kind = field_kind_from_name(type_name);
        if (!kind) {
            luaL_error(L, "%s.%s: unknown type \"%s\" (f32, i32, u32, bool, vec2, vec3, vec4, quat, entity)", name.c_str(), d.name.c_str(),
                       type_name.c_str());
        }
        d.kind = *kind;
        if (d.kind == FieldKind::Vec2 || d.kind == FieldKind::Vec3 || d.kind == FieldKind::Vec4 || d.kind == FieldKind::Quat) {
            if (d.default_value.is_object()) { // {x=, y=} -> [x, y]
                Json a = Json::array();
                for (const char* axis : {"x", "y", "z", "w"}) {
                    if (const Json* v = d.default_value.find(axis)) a.push_back(*v);
                }
                d.default_value = a;
            }
        }
        defs.push_back(std::move(d));
        lua_pop(L, 1);
    }
    std::sort(defs.begin(), defs.end(), [](const FieldDef& a, const FieldDef& b) { return a.name < b.name; });
    if (const TypeRegistry::Entry* existing = impl.env.types.find(name)) { // reloading the module that defined it
        bool same = existing->dynamic && existing->fields.size() == defs.size();
        for (std::size_t i = 0; same && i < defs.size(); ++i) same = existing->fields[i].name == defs[i].name && existing->fields[i].kind == defs[i].kind;
        if (!same) luaL_error(L, "%s is already defined differently; a component's fields cannot change while the game runs (restart)", name.c_str());
        return 0;
    }
    std::string error;
    if (!impl.env.types.add_dynamic(name, std::move(defs), &error)) luaL_error(L, "%s", error.c_str());
    return 0;
}

int astral_define_event(lua_State* L) {
    Impl::of(L).env.events.add_dynamic(luaL_checkstring(L, 1));
    return 0;
}

int astral_spawn(lua_State* L) {
    Impl& impl = Impl::of(L);
    luaL_checktype(L, 1, LUA_TTABLE);
    const Entity e = impl.env.world.create();
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        if (lua_type(L, -2) != LUA_TSTRING) luaL_error(L, "spawn: use {[\"component name\"] = {fields}}");
        const std::uint32_t type = require_type(L, impl, lua_tostring(L, -2));
        add_component(L, impl, e, type, lua_gettop(L));
        lua_pop(L, 1);
    }
    push_entity(L, e);
    return 1;
}

/// astral.query({"A", "B"}) -> { {entity, a, b}, ... }: every entity that has all of them, with live proxies.
int astral_query(lua_State* L) {
    Impl& impl = Impl::of(L);
    luaL_checktype(L, 1, LUA_TTABLE);
    std::vector<std::uint32_t> types;
    std::vector<ComponentId> ids;
    const int n = lua_objlen(L, 1);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 1, i);
        if (lua_type(L, -1) != LUA_TSTRING) luaL_error(L, "query: expected a list of component names");
        const std::uint32_t t = require_type(L, impl, lua_tostring(L, -1));
        lua_pop(L, 1);
        types.push_back(t);
        ids.push_back(entry_of(impl, t).component);
    }
    std::vector<Entity> entities;
    impl.env.world.each_raw(ids, [&](Entity e) { entities.push_back(e); });
    lua_createtable(L, static_cast<int>(entities.size()), 0);
    int row = 1;
    for (Entity e : entities) {
        lua_createtable(L, static_cast<int>(types.size()) + 1, 0);
        push_entity(L, e);
        lua_rawseti(L, -2, 1);
        for (std::size_t i = 0; i < types.size(); ++i) {
            push_component(L, e, types[i]);
            lua_rawseti(L, -2, static_cast<int>(i) + 2);
        }
        lua_rawseti(L, -2, row++);
    }
    return 1;
}

/// astral.each({"A", "B"}, function(entity, a, b) ... end) -> number of visited entities.
/// Streams matching entities directly into the callback without allocating intermediate Lua tables (ADR-0032).
int astral_each(lua_State* L) {
    Impl& impl = Impl::of(L);
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    std::vector<std::uint32_t> types;
    std::vector<ComponentId> ids;
    const int n = lua_objlen(L, 1);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 1, i);
        if (lua_type(L, -1) != LUA_TSTRING) luaL_error(L, "each: expected a list of component names");
        const std::uint32_t t = require_type(L, impl, lua_tostring(L, -1));
        lua_pop(L, 1);
        types.push_back(t);
        ids.push_back(entry_of(impl, t).component);
    }
    std::vector<Entity> entities;
    impl.env.world.each_raw(ids, [&](Entity e) { entities.push_back(e); });
    std::size_t visited = 0;
    for (Entity e : entities) {
        bool ok = impl.env.world.alive(e);
        for (std::size_t i = 0; ok && i < ids.size(); ++i) ok = impl.env.world.has_raw(e, ids[i]);
        if (!ok) continue;
        lua_pushvalue(L, 2);
        push_entity(L, e);
        for (std::uint32_t t : types) push_component(L, e, t);
        lua_call(L, 1 + static_cast<int>(types.size()), 0);
        ++visited;
    }
    lua_pushnumber(L, static_cast<double>(visited));
    return 1;
}

int astral_find(lua_State* L) {
    Impl& impl = Impl::of(L);
    const Entity e = find_by_name(impl.env.world, luaL_checkstring(L, 1));
    if (e.valid()) push_entity(L, e);
    else lua_pushnil(L);
    return 1;
}

int astral_send(lua_State* L) {
    Impl& impl = Impl::of(L);
    const std::string event = luaL_checkstring(L, 1);
    const EventRegistry::Entry* entry = impl.env.events.find(event);
    if (!entry) luaL_error(L, "unknown event \"%s\" (define it with astral.define_event or events.add<T>() in C++)", event.c_str());
    const Json payload = lua_isnoneornil(L, 2) ? Json::object() : lua_to_json(L, 2, &entry->fields);
    std::vector<std::string> errors;
    entry->send(impl.env.world, payload, &errors);
    if (!errors.empty()) luaL_error(L, "%s", errors.front().c_str());
    return 0;
}

int add_timer(lua_State* L, bool repeating) {
    Impl& impl = Impl::of(L);
    Module& m = active_module(L, repeating ? "astral.every" : "astral.after");
    Timer t;
    t.id = impl.next_timer++;
    t.owner = &m;
    t.remaining = luaL_checknumber(L, 1);
    t.interval = repeating ? std::max(0.001, t.remaining) : 0.0;
    t.fn_ref = function_ref(L, 2);
    impl.timers.push_back(std::move(t));
    lua_pushnumber(L, impl.timers.back().id);
    return 1;
}
int astral_after(lua_State* L) { return add_timer(L, false); }
int astral_every(lua_State* L) { return add_timer(L, true); }

int astral_cancel(lua_State* L) {
    Impl& impl = Impl::of(L);
    const auto id = static_cast<std::uint32_t>(luaL_checknumber(L, 1));
    for (auto it = impl.timers.begin(); it != impl.timers.end(); ++it) {
        if (it->id == id) {
            lua_unref(L, it->fn_ref);
            impl.timers.erase(it);
            break;
        }
    }
    return 0;
}

int astral_random(lua_State* L) {
    Impl& impl = Impl::of(L);
    lua_pushnumber(L, static_cast<double>(impl.rng() - 1) / static_cast<double>(std::minstd_rand::max() - 1));
    return 1;
}

int astral_seed(lua_State* L) {
    Impl::of(L).rng.seed(static_cast<std::uint32_t>(luaL_checknumber(L, 1)));
    return 0;
}

std::string joined(lua_State* L) {
    std::string out;
    const int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        size_t len = 0;
        const char* s = luaL_tolstring(L, i, &len);
        if (i > 1) out += '\t';
        out.append(s, len);
        lua_pop(L, 1);
    }
    return out;
}
int log_info(lua_State* L) { return Impl::of(L).env.log.log(LogLevel::Info, joined(L)), 0; }
int log_warn(lua_State* L) { return Impl::of(L).env.log.log(LogLevel::Warn, joined(L)), 0; }
int log_error(lua_State* L) { return Impl::of(L).env.log.log(LogLevel::Error, joined(L)), 0; }

void make_metatable(lua_State* L, int tag, const char* type_name) {
    lua_newtable(L);
    lua_pushstring(L, type_name);
    lua_setfield(L, -2, "__type");
    lua_setuserdatametatable(L, tag); // pops it; getuserdatametatable gives it back below
    lua_getuserdatametatable(L, tag);
}

} // namespace

void open_astral(lua_State* L, ScriptHost::Impl&) {
    // Entities.
    make_metatable(L, tag_entity, "entity");
    lua_newtable(L); // methods
    set_function(L, "get", entity_get);
    set_function(L, "has", entity_has);
    set_function(L, "add", entity_add);
    set_function(L, "remove", entity_remove);
    set_function(L, "destroy", entity_destroy);
    set_function(L, "alive", entity_alive);
    set_function(L, "id", entity_id);
    lua_setfield(L, -2, "__index");
    set_function(L, "__eq", entity_eq);
    set_function(L, "__tostring", entity_tostring);
    lua_pop(L, 1);
    // Component proxies.
    make_metatable(L, tag_component, "component");
    set_function(L, "__index", component_index);
    set_function(L, "__newindex", component_newindex);
    set_function(L, "__tostring", component_tostring);
    lua_pop(L, 1);

    lua_newtable(L); // astral
    set_function(L, "system", astral_system);
    set_function(L, "on", astral_on);
    set_function(L, "behaviour", astral_behaviour);
    set_function(L, "define_component", astral_define_component);
    set_function(L, "define_event", astral_define_event);
    set_function(L, "spawn", astral_spawn);
    set_function(L, "find", astral_find);
    set_function(L, "query", astral_query);
    set_function(L, "each", astral_each);
    set_function(L, "send", astral_send);
    set_function(L, "after", astral_after);
    set_function(L, "every", astral_every);
    set_function(L, "cancel", astral_cancel);
    set_function(L, "random", astral_random);
    set_function(L, "seed", astral_seed);
    set_function(L, "log", log_info);
    set_function(L, "warn", log_warn);
    set_function(L, "error", log_error);
    register_service_bindings(L);
    lua_setglobal(L, "astral");

    lua_pushcfunction(L, log_info, "print");
    lua_setglobal(L, "print");
}

} // namespace astral
