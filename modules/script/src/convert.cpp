// Lua <-> engine values: fields, entities, JSON.
#include "host_impl.hpp"

#include <cmath>
#include <cstring>

namespace astral {

namespace {

template <class T> T load(const void* p) noexcept {
    T v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void push_floats(lua_State* L, const float* f, std::size_t n) {
    lua_createtable(L, 0, static_cast<int>(n));
    constexpr const char* names[] = {"x", "y", "z", "w"};
    for (std::size_t i = 0; i < n; ++i) {
        lua_pushnumber(L, static_cast<double>(f[i]));
        lua_setfield(L, -2, names[i]);
    }
}

/// Reads x, y, (z, w) from a vector or a table into out[0..n).
bool read_floats(lua_State* L, int index, float* out, std::size_t n) {
    if (lua_isvector(L, index)) {
        const float* v = lua_tovector(L, index);
        for (std::size_t i = 0; i < n && i < 3; ++i) out[i] = v[i];
        return n <= 3;
    }
    if (!lua_istable(L, index)) return false;
    constexpr const char* names[] = {"x", "y", "z", "w"};
    for (std::size_t i = 0; i < n; ++i) {
        lua_getfield(L, index, names[i]);
        const bool ok = lua_isnumber(L, -1) != 0;
        if (ok) out[i] = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        if (!ok) return false;
    }
    return true;
}

const char* kind_label(FieldKind k) {
    switch (k) {
    case FieldKind::Bool: return "a boolean";
    case FieldKind::Int:
    case FieldKind::UInt:
    case FieldKind::Float: return "a number";
    case FieldKind::Vec2: return "a vec2 ({x, y})";
    case FieldKind::Vec3: return "a vector";
    case FieldKind::Vec4: return "a vec4 ({x, y, z, w})";
    case FieldKind::Quat: return "a quaternion ({x, y, z, w})";
    case FieldKind::EntityRef: return "an entity or nil";
    case FieldKind::Enum: return "an enumerator name (string)";
    case FieldKind::String: return "a string";
    default: return "a value";
    }
}

} // namespace

bool read_floats_checked(lua_State* L, int index, float out[4]) { return read_floats(L, index, out, 4); }

void push_entity(lua_State* L, Entity e) {
    auto* box = static_cast<EntityBox*>(lua_newuserdatataggedwithmetatable(L, sizeof(EntityBox), tag_entity));
    box->entity = e;
}

bool read_entity(lua_State* L, int index, Entity& out) {
    if (lua_isnil(L, index)) {
        out = {};
        return true;
    }
    if (lua_touserdatatagged(L, index, tag_entity)) {
        out = static_cast<EntityBox*>(lua_touserdatatagged(L, index, tag_entity))->entity;
        return true;
    }
    return false;
}

void push_field(lua_State* L, const FieldInfo& f, const void* base, const World& world) {
    const auto* p = static_cast<const std::byte*>(base) + f.offset;
    switch (f.kind) {
    case FieldKind::Bool: lua_pushboolean(L, read_number(f, base) != 0.0); return;
    case FieldKind::Int:
    case FieldKind::UInt:
    case FieldKind::Float: lua_pushnumber(L, read_number(f, base)); return;
    case FieldKind::Enum: {
        const auto value = static_cast<std::int64_t>(read_number(f, base));
        for (const auto& [name, v] : f.enumerators) {
            if (v == value) return lua_pushlstring(L, name.data(), name.size());
        }
        lua_pushnumber(L, static_cast<double>(value));
        return;
    }
    case FieldKind::Vec2: push_floats(L, reinterpret_cast<const float*>(p), 2); return;
    case FieldKind::Vec3: {
        const auto* v = reinterpret_cast<const float*>(p);
        lua_pushvector(L, v[0], v[1], v[2]);
        return;
    }
    case FieldKind::Vec4:
    case FieldKind::Quat: push_floats(L, reinterpret_cast<const float*>(p), 4); return;
    case FieldKind::EntityRef: {
        const Entity e = load<Entity>(p);
        if (world.alive(e)) push_entity(L, e);
        else lua_pushnil(L);
        return;
    }
    case FieldKind::String: {
        const auto* s = reinterpret_cast<const std::string*>(p);
        lua_pushlstring(L, s->data(), s->size());
        return;
    }
    default: lua_pushnil(L); return;
    }
}

bool read_field(lua_State* L, int index, const FieldInfo& f, void* base, std::string* error) {
    auto* p = static_cast<std::byte*>(base) + f.offset;
    const auto fail = [&] {
        if (error) *error = "field \"" + f.name + "\" expects " + kind_label(f.kind) + ", got " + luaL_typename(L, index);
        return false;
    };
    switch (f.kind) {
    case FieldKind::Bool:
        if (!lua_isboolean(L, index)) return fail();
        write_number(f, base, lua_toboolean(L, index) ? 1.0 : 0.0);
        return true;
    case FieldKind::Int:
    case FieldKind::UInt:
    case FieldKind::Float:
        if (lua_type(L, index) != LUA_TNUMBER) return fail();
        write_number(f, base, lua_tonumber(L, index));
        return true;
    case FieldKind::Enum:
        if (lua_type(L, index) == LUA_TSTRING) {
            const std::string name = lua_tostring(L, index);
            for (const auto& [n, v] : f.enumerators) {
                if (n == name) return write_number(f, base, static_cast<double>(v)), true;
            }
            if (error) {
                *error = "field \"" + f.name + "\": unknown enumerator \"" + name + "\" (use:";
                for (const auto& [n, v] : f.enumerators) *error += " " + n;
                *error += ")";
            }
            return false;
        }
        return fail();
    case FieldKind::Vec2: {
        float v[2];
        if (!read_floats(L, index, v, 2)) return fail();
        std::memcpy(p, v, sizeof v);
        return true;
    }
    case FieldKind::Vec3: {
        float v[3];
        if (!read_floats(L, index, v, 3)) return fail();
        std::memcpy(p, v, sizeof v);
        return true;
    }
    case FieldKind::Vec4:
    case FieldKind::Quat: {
        float v[4];
        if (!read_floats(L, index, v, 4)) return fail();
        std::memcpy(p, v, sizeof v);
        return true;
    }
    case FieldKind::EntityRef: {
        Entity e;
        if (!read_entity(L, index, e)) return fail();
        std::memcpy(p, &e, sizeof e);
        return true;
    }
    case FieldKind::String:
        if (lua_type(L, index) != LUA_TSTRING) return fail();
        *reinterpret_cast<std::string*>(p) = lua_tostring(L, index);
        return true;
    default:
        if (error) *error = "field \"" + f.name + "\" cannot be set from a script";
        return false;
    }
}

namespace {

const FieldInfo* member_hint(const std::vector<FieldInfo>* fields, std::string_view name) {
    return fields ? find_field(*fields, name) : nullptr;
}

Json floats_json(lua_State* L, int index, std::size_t n) {
    float v[4]{};
    if (!read_floats(L, index, v, n)) return nullptr;
    return detail::floats(v, n);
}

/// Deepest table nesting converted to JSON; deeper input (or a table that contains itself) is an
/// error, so a payload can never recurse the host's C++ stack without bound.
constexpr int max_json_depth = 64;

Json table_to_json(lua_State* L, int index, const std::vector<FieldInfo>* fields, int depth);

Json value_to_json(lua_State* L, int index, const FieldInfo* hint, int depth) {
    index = lua_absindex(L, index);
    switch (lua_type(L, index)) {
    case LUA_TNIL: return nullptr;
    case LUA_TBOOLEAN: return lua_toboolean(L, index) != 0;
    case LUA_TNUMBER: return lua_tonumber(L, index);
    case LUA_TSTRING: return std::string(lua_tostring(L, index));
    case LUA_TVECTOR: return floats_json(L, index, 3);
    case LUA_TUSERDATA: {
        Entity e;
        if (read_entity(L, index, e) && e.valid()) return "#" + std::to_string(e.index) + "." + std::to_string(e.generation);
        return nullptr;
    }
    case LUA_TTABLE: {
        if (hint) {
            switch (hint->kind) {
            case FieldKind::Vec2: return floats_json(L, index, 2);
            case FieldKind::Vec3: return floats_json(L, index, 3);
            case FieldKind::Vec4:
            case FieldKind::Quat: return floats_json(L, index, 4);
            default: break;
            }
        }
        return table_to_json(L, index, nullptr, depth + 1);
    }
    default: return nullptr;
    }
}

Json table_to_json(lua_State* L, int index, const std::vector<FieldInfo>* fields, int depth) {
    if (depth > max_json_depth) luaL_error(L, "table is nested deeper than %d levels (or contains itself)", max_json_depth);
    if (!lua_checkstack(L, 3)) luaL_error(L, "not enough Lua stack to convert a table");
    const int length = lua_objlen(L, index);
    if (length > 0) { // an array
        Json a = Json::array();
        for (int i = 1; i <= length; ++i) {
            lua_rawgeti(L, index, i);
            a.push_back(value_to_json(L, -1, nullptr, depth));
            lua_pop(L, 1);
        }
        return a;
    }
    Json o = Json::object();
    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            const std::string key = lua_tostring(L, -2);
            o[key] = value_to_json(L, -1, member_hint(fields, key), depth);
        }
        lua_pop(L, 1);
    }
    return o;
}

} // namespace

Json lua_to_json(lua_State* L, int index, const std::vector<FieldInfo>* fields) {
    index = lua_absindex(L, index);
    if (lua_type(L, index) != LUA_TTABLE) return value_to_json(L, index, nullptr, 0);
    return table_to_json(L, index, fields, 0);
}

void json_to_lua(lua_State* L, const Json& j, const std::vector<FieldInfo>* fields, World& world) {
    switch (j.type()) {
    case Json::Type::Null: lua_pushnil(L); return;
    case Json::Type::Bool: lua_pushboolean(L, j.as_bool()); return;
    case Json::Type::Number: lua_pushnumber(L, j.as_number()); return;
    case Json::Type::String: {
        const std::string& s = j.as_string();
        if (s.size() > 3 && s[0] == '#' && s.find('.') != std::string::npos) { // a live entity reference
            const LoadContext ctx = live_load_context(world, nullptr);
            Entity e;
            if (ctx.resolve(j, e)) return push_entity(L, e);
        }
        lua_pushlstring(L, s.data(), s.size());
        return;
    }
    case Json::Type::Array:
        lua_createtable(L, static_cast<int>(j.size()), 0);
        for (std::size_t i = 0; i < j.size(); ++i) {
            json_to_lua(L, j[i], nullptr, world);
            lua_rawseti(L, -2, static_cast<int>(i) + 1);
        }
        return;
    case Json::Type::Object:
        lua_createtable(L, 0, static_cast<int>(j.size()));
        for (const auto& [key, value] : j.as_object()) {
            const FieldInfo* hint = member_hint(fields, key);
            if (hint && (hint->kind == FieldKind::Vec3) && value.is_array() && value.size() == 3) {
                lua_pushvector(L, static_cast<float>(value[0].as_number()), static_cast<float>(value[1].as_number()),
                               static_cast<float>(value[2].as_number()));
            } else if (hint && (hint->kind == FieldKind::Vec2 || hint->kind == FieldKind::Vec4 || hint->kind == FieldKind::Quat) &&
                       value.is_array()) {
                constexpr const char* names[] = {"x", "y", "z", "w"};
                lua_createtable(L, 0, 4);
                for (std::size_t i = 0; i < value.size() && i < 4; ++i) {
                    lua_pushnumber(L, value[i].as_number());
                    lua_setfield(L, -2, names[i]);
                }
            } else {
                json_to_lua(L, value, nullptr, world);
            }
            lua_setfield(L, -2, key.c_str());
        }
        return;
    }
}

} // namespace astral
