// The `astral` service bindings: quat, save/load, physics, fields, and input.
#include "bindings_internal.hpp"

#include <astral/world/scene.hpp>

namespace astral::bindings_detail {

namespace {

// --- quaternions: tables {x, y, z, w} (what Quat fields are in scripts) ----------------------------

int quat_axis_angle(lua_State* L) {
    const float* axis = lua_tovector(L, 1);
    if (!axis) luaL_typeerror(L, 1, "vector");
    push_quat(L, Quat::from_axis_angle({axis[0], axis[1], axis[2]}, static_cast<float>(luaL_checknumber(L, 2))));
    return 1;
}
int quat_mul(lua_State* L) {
    push_quat(L, check_quat(L, 1) * check_quat(L, 2));
    return 1;
}
int quat_normalize(lua_State* L) {
    push_quat(L, normalize(check_quat(L, 1)));
    return 1;
}
int quat_rotate(lua_State* L) {
    const float* v = lua_tovector(L, 2);
    if (!v) luaL_typeerror(L, 2, "vector");
    const Vec3 r = check_quat(L, 1) * Vec3{v[0], v[1], v[2]};
    lua_pushvector(L, r.x, r.y, r.z);
    return 1;
}

// --- saves: selected components of the world as a named slot ---------------------------------------

/// A registry with only the listed component types (a table of names at `index`).
TypeRegistry subset_registry(lua_State* L, Impl& impl, int index, std::vector<std::string>& names) {
    luaL_checktype(L, index, LUA_TTABLE);
    TypeRegistry subset;
    const int n = lua_objlen(L, index);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, index, i);
        if (lua_type(L, -1) != LUA_TSTRING) luaL_error(L, "save: expected a list of component names");
        const std::string name = lua_tostring(L, -1);
        lua_pop(L, 1);
        const std::uint32_t t = require_type(L, impl, name);
        subset.add_copy(entry_of(impl, t));
        names.push_back(name);
    }
    return subset;
}

ScriptStorage& require_storage(lua_State* L, Impl& impl) {
    if (!impl.env.storage) luaL_error(L, "saving is not available in this game (no storage)");
    return *impl.env.storage;
}

int astral_save(lua_State* L) {
    Impl& impl = Impl::of(L);
    const std::string slot = luaL_checkstring(L, 1);
    std::vector<std::string> names;
    const TypeRegistry subset = subset_registry(L, impl, 2, names);
    Json doc = Json::object();
    doc["format"] = "astral.save";
    doc["version"] = 1;
    Json list = Json::array();
    for (const std::string& n : names) list.push_back(n);
    doc["components"] = std::move(list);
    doc["world"] = save_world(impl.env.world, subset);
    lua_pushboolean(L, require_storage(L, impl).write(slot, doc.dump(2)));
    return 1;
}

int astral_has_save(lua_State* L) {
    Impl& impl = Impl::of(L);
    lua_pushboolean(L, require_storage(L, impl).read(luaL_checkstring(L, 1)).has_value());
    return 1;
}

/// Replaces the entities that have any saved component with the slot's: they are destroyed, then loaded again.
int astral_load(lua_State* L) {
    Impl& impl = Impl::of(L);
    const std::string slot = luaL_checkstring(L, 1);
    const auto text = require_storage(L, impl).read(slot);
    if (!text) {
        lua_pushboolean(L, false);
        return 1;
    }
    std::string parse_error;
    const auto doc = Json::parse(*text, &parse_error);
    if (!doc || (*doc)["format"].as_string() != "astral.save") luaL_error(L, "save \"%s\" is not a valid save (%s)", slot.c_str(), parse_error.c_str());
    TypeRegistry subset;
    std::vector<ComponentId> ids;
    for (const Json& n : (*doc)["components"].as_array()) {
        const TypeRegistry::Entry* e = impl.env.types.find(n.as_string());
        if (!e) luaL_error(L, "save \"%s\" has the unknown component \"%s\" (the game changed since it was saved)", slot.c_str(), n.as_string().c_str());
        subset.add_copy(*e);
        ids.push_back(e->component);
    }
    std::vector<Entity> doomed;
    for (const auto& e : subset.entries()) {
        impl.env.world.each_raw(std::span<const ComponentId>(&e.component, 1), [&](Entity x) { doomed.push_back(x); });
    }
    for (Entity e : doomed) {
        if (impl.env.world.alive(e)) impl.env.world.destroy(e);
    }
    const LoadResult r = load_world(impl.env.world, subset, (*doc)["world"]);
    if (!r.errors.empty()) luaL_error(L, "loading \"%s\": %s", slot.c_str(), r.errors.front().c_str());
    lua_pushboolean(L, true);
    return 1;
}

// --- physics and fields ----------------------------------------------------------------------

ScriptPhysics& require_physics(lua_State* L, Impl& impl) {
    if (!impl.env.physics) luaL_error(L, "physics is not available in this game");
    return *impl.env.physics;
}

Vec3 check_vec3(lua_State* L, int index) {
    const float* v = lua_tovector(L, index);
    if (!v) luaL_typeerror(L, index, "vector");
    return {v[0], v[1], v[2]};
}

void push_vec3(lua_State* L, Vec3 v) { lua_pushvector(L, v.x, v.y, v.z); }

/// options.layers: a mask (number) or a list of layer indices {0, 3}; options.ignore: an entity.
void read_query_options(lua_State* L, int index, std::uint32_t& layers, Entity& ignore) {
    layers = 0xFFFFFFFFu;
    ignore = {};
    if (lua_isnoneornil(L, index)) return;
    luaL_checktype(L, index, LUA_TTABLE);
    lua_getfield(L, index, "layers");
    if (lua_type(L, -1) == LUA_TNUMBER) {
        layers = static_cast<std::uint32_t>(lua_tonumber(L, -1));
    } else if (lua_istable(L, -1)) {
        layers = 0;
        const int n = lua_objlen(L, -1);
        for (int i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            const int layer = static_cast<int>(luaL_checknumber(L, -1));
            if (layer < 0 || layer > 15) luaL_error(L, "layers are numbered 0..15");
            layers |= 1u << layer;
            lua_pop(L, 1);
        }
    } else if (!lua_isnil(L, -1)) {
        luaL_error(L, "options.layers must be a number mask or a list of layer numbers");
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "ignore");
    if (!lua_isnil(L, -1) && !read_entity(L, -1, ignore)) luaL_error(L, "options.ignore must be an entity");
    lua_pop(L, 1);
}

void push_hit(lua_State* L, const ScriptHit& hit) {
    lua_createtable(L, 0, 5);
    push_entity(L, hit.entity);
    lua_setfield(L, -2, "entity");
    push_vec3(L, hit.position);
    lua_setfield(L, -2, "position");
    push_vec3(L, hit.normal);
    lua_setfield(L, -2, "normal");
    lua_pushnumber(L, static_cast<double>(hit.distance));
    lua_setfield(L, -2, "distance");
    lua_pushnumber(L, hit.surface);
    lua_setfield(L, -2, "surface");
}

int physics_raycast(lua_State* L) {
    ScriptPhysics& p = require_physics(L, Impl::of(L));
    std::uint32_t layers;
    Entity ignore;
    read_query_options(L, 4, layers, ignore);
    const auto hit = p.raycast(check_vec3(L, 1), check_vec3(L, 2), static_cast<float>(luaL_checknumber(L, 3)), layers, ignore);
    if (hit) push_hit(L, *hit);
    else lua_pushnil(L);
    return 1;
}

int physics_spherecast(lua_State* L) {
    ScriptPhysics& p = require_physics(L, Impl::of(L));
    std::uint32_t layers;
    Entity ignore;
    read_query_options(L, 4, layers, ignore);
    const auto hit = p.spherecast(check_vec3(L, 1), static_cast<float>(luaL_checknumber(L, 2)), check_vec3(L, 3), layers, ignore);
    if (hit) push_hit(L, *hit);
    else lua_pushnil(L);
    return 1;
}

void push_entities(lua_State* L, const std::vector<Entity>& list) {
    lua_createtable(L, static_cast<int>(list.size()), 0);
    int i = 1;
    for (Entity e : list) {
        push_entity(L, e);
        lua_rawseti(L, -2, i++);
    }
}

int physics_overlap_sphere(lua_State* L) {
    ScriptPhysics& p = require_physics(L, Impl::of(L));
    std::uint32_t layers;
    Entity ignore;
    read_query_options(L, 3, layers, ignore);
    push_entities(L, p.overlap_sphere(check_vec3(L, 1), static_cast<float>(luaL_checknumber(L, 2)), layers));
    return 1;
}

int physics_overlap_box(lua_State* L) {
    ScriptPhysics& p = require_physics(L, Impl::of(L));
    std::uint32_t layers;
    Entity ignore;
    read_query_options(L, 4, layers, ignore);
    Quat rotation{};
    if (!lua_isnoneornil(L, 3)) rotation = check_quat(L, 3);
    push_entities(L, p.overlap_box(check_vec3(L, 1), check_vec3(L, 2), rotation, layers));
    return 1;
}

int physics_impulse(lua_State* L) {
    require_physics(L, Impl::of(L)).impulse(check_entity(L, 1), check_vec3(L, 2));
    return 0;
}
int physics_force(lua_State* L) {
    require_physics(L, Impl::of(L)).force(check_entity(L, 1), check_vec3(L, 2));
    return 0;
}
int physics_set_velocity(lua_State* L) {
    require_physics(L, Impl::of(L)).set_velocity(check_entity(L, 1), check_vec3(L, 2));
    return 0;
}
int physics_velocity(lua_State* L) {
    push_vec3(L, require_physics(L, Impl::of(L)).velocity(check_entity(L, 1)));
    return 1;
}

int fields_wind(lua_State* L) {
    push_vec3(L, require_physics(L, Impl::of(L)).wind(check_vec3(L, 1)));
    return 1;
}

int fields_water(lua_State* L) {
    const ScriptWater w = require_physics(L, Impl::of(L)).water(check_vec3(L, 1));
    lua_createtable(L, 0, 5);
    lua_pushboolean(L, w.inside);
    lua_setfield(L, -2, "inside");
    lua_pushnumber(L, static_cast<double>(w.surface));
    lua_setfield(L, -2, "surface");
    lua_pushnumber(L, static_cast<double>(w.depth));
    lua_setfield(L, -2, "depth");
    push_vec3(L, w.flow);
    lua_setfield(L, -2, "flow");
    lua_pushnumber(L, static_cast<double>(w.density));
    lua_setfield(L, -2, "density");
    return 1;
}

// --- input ------------------------------------------------------------------------------------

int input_down(lua_State* L) {
    ScriptInput* in = Impl::of(L).env.input;
    lua_pushboolean(L, in && in->down(luaL_checkstring(L, 1)));
    return 1;
}
int input_pressed(lua_State* L) {
    ScriptInput* in = Impl::of(L).env.input;
    lua_pushboolean(L, in && in->pressed(luaL_checkstring(L, 1)));
    return 1;
}
int input_released(lua_State* L) {
    ScriptInput* in = Impl::of(L).env.input;
    lua_pushboolean(L, in && in->released(luaL_checkstring(L, 1)));
    return 1;
}
int input_value(lua_State* L) {
    ScriptInput* in = Impl::of(L).env.input;
    lua_pushnumber(L, in ? static_cast<double>(in->value(luaL_checkstring(L, 1))) : 0.0);
    return 1;
}
int input_axis(lua_State* L) {
    ScriptInput* in = Impl::of(L).env.input;
    const Vec2 a = in ? in->axis(luaL_checkstring(L, 1)) : Vec2{};
    lua_pushvector(L, a.x, a.y, 0.0f);
    return 1;
}

} // namespace

void register_service_bindings(lua_State* L) {
    set_function(L, "save", astral_save);
    set_function(L, "load", astral_load);
    set_function(L, "has_save", astral_has_save);

    lua_newtable(L); // astral.physics
    set_function(L, "raycast", physics_raycast);
    set_function(L, "spherecast", physics_spherecast);
    set_function(L, "overlap_sphere", physics_overlap_sphere);
    set_function(L, "overlap_box", physics_overlap_box);
    set_function(L, "impulse", physics_impulse);
    set_function(L, "force", physics_force);
    set_function(L, "set_velocity", physics_set_velocity);
    set_function(L, "velocity", physics_velocity);
    lua_setfield(L, -2, "physics");

    lua_newtable(L); // astral.fields
    set_function(L, "wind", fields_wind);
    set_function(L, "water", fields_water);
    lua_setfield(L, -2, "fields");

    lua_newtable(L); // astral.quat
    set_function(L, "axis_angle", quat_axis_angle);
    set_function(L, "mul", quat_mul);
    set_function(L, "normalize", quat_normalize);
    set_function(L, "rotate", quat_rotate);
    lua_setfield(L, -2, "quat");

    lua_newtable(L); // astral.input
    set_function(L, "down", input_down);
    set_function(L, "pressed", input_pressed);
    set_function(L, "released", input_released);
    set_function(L, "value", input_value);
    set_function(L, "axis", input_axis);
    lua_setfield(L, -2, "input");
}

} // namespace astral::bindings_detail
