#pragma once
// World <-> JSON through reflection (ASTRAL_REFLECT). Only components registered in a
// TypeRegistry are saved; entity references inside components are remapped on load.
//
//     TypeRegistry types;
//     register_world_types(types);        // Transform, Parent
//     types.add<Health>();                // any ASTRAL_REFLECT'ed component
//     Json doc = save_world(world, types);
//     LoadResult r = load_world(other, types, doc);   // appends; r.errors lists problems
//
// Format: {"format": "astral.world", "version": 1, "entities": [{"id": N, "components": {...}}]}.
// This is a flat snapshot (savegames, tools): prefab instances are saved as plain entities. Scenes
// and prefabs, which keep instance links and overrides, use scene.hpp.
// Loading is lenient: unknown fields are ignored, missing fields keep their defaults, unknown
// components and bad values are reported in LoadResult::errors and skipped.

#include <astral/core/json.hpp>
#include <astral/core/math.hpp>
#include <astral/core/reflect.hpp>
#include <astral/world/fields.hpp>
#include <astral/world/transform.hpp>
#include <astral/world/world.hpp>

#include <array>
#include <charconv>
#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

ASTRAL_REFLECT(astral::Transform, position, rotation, scale)
ASTRAL_REFLECT(astral::Parent, entity)

namespace astral {

/// How entity references are written while saving (file ids, scene paths...). Unset or returning
/// null: the reference is saved as null (the entity is not part of the document).
struct SaveContext {
    std::function<Json(Entity)> ref;
    [[nodiscard]] Json entity(Entity e) const {
        if (!e.valid() || !ref) return nullptr;
        return ref(e);
    }
};

/// How entity references are read while loading; collects problems with a readable path.
struct LoadContext {
    /// Non-null reference -> entity. False: the reference names nothing in this document.
    std::function<bool(const Json&, Entity&)> resolve;
    std::vector<std::string>* errors = nullptr;
    std::string path;
    void error(std::string_view message) const {
        if (errors) errors->push_back(path + ": " + std::string(message));
    }
};

template <class T> struct is_vector : std::false_type {};
template <class U, class A> struct is_vector<std::vector<U, A>> : std::true_type {};
template <class T> struct is_optional : std::false_type {};
template <class U> struct is_optional<std::optional<U>> : std::true_type {};
template <class T> struct is_std_array : std::false_type {};
template <class U, std::size_t N> struct is_std_array<std::array<U, N>> : std::true_type {};
template <class T> struct is_handle : std::false_type {};
template <class Tag> struct is_handle<Handle<Tag>> : std::true_type {};

template <class T> Json to_json(const T& value, const SaveContext& ctx);
template <class T> bool from_json(const Json& j, T& value, const LoadContext& ctx);

namespace detail {
/// The shortest decimal that reads back as the same float (0.6f saves as 0.6, not 0.6000000238...).
inline double float_value(float f) {
    char buf[32];
    const auto [end, ec] = std::to_chars(buf, buf + sizeof buf, f);
    double d = f;
    if (ec == std::errc{}) std::from_chars(buf, end, d);
    return d;
}
inline Json floats(const float* f, std::size_t n) {
    Json a = Json::array();
    for (std::size_t i = 0; i < n; ++i) a.push_back(float_value(f[i]));
    return a;
}
inline bool read_floats(const Json& j, float* f, std::size_t n, const LoadContext& ctx) {
    if (!j.is_array() || j.size() != n) {
        ctx.error("expected an array of " + std::to_string(n) + " numbers");
        return false;
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (!j[i].is_number()) {
            ctx.error("expected numbers");
            return false;
        }
    }
    for (std::size_t i = 0; i < n; ++i) f[i] = static_cast<float>(j[i].as_number());
    return true;
}
} // namespace detail

template <class T> Json to_json(const T& v, const SaveContext& ctx) {
    if constexpr (std::is_same_v<T, Entity>) {
        return ctx.entity(v);
    } else if constexpr (is_handle<T>::value) {
        return nullptr; // runtime resource handles are meaningless in a file (assets use AssetIds)
    } else if constexpr (std::is_same_v<T, bool>) {
        return v;
    } else if constexpr (std::is_enum_v<T>) {
        if constexpr (is_reflected_enum_v<T>) {
            if (const std::string_view name = enum_name(v); !name.empty()) return name;
        }
        return static_cast<double>(static_cast<std::underlying_type_t<T>>(v));
    } else if constexpr (std::is_same_v<T, float>) {
        return detail::float_value(v);
    } else if constexpr (std::is_arithmetic_v<T>) {
        return static_cast<double>(v);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return v;
    } else if constexpr (std::is_same_v<T, Vec2>) {
        return detail::floats(&v.x, 2);
    } else if constexpr (std::is_same_v<T, Vec3>) {
        return detail::floats(&v.x, 3);
    } else if constexpr (std::is_same_v<T, Vec4>) {
        return detail::floats(&v.x, 4);
    } else if constexpr (std::is_same_v<T, Quat>) {
        return detail::floats(&v.x, 4);
    } else if constexpr (std::is_same_v<T, Mat4>) {
        return detail::floats(&v[0].x, 16);
    } else if constexpr (is_optional<T>::value) {
        return v ? to_json(*v, ctx) : Json(nullptr);
    } else if constexpr (is_vector<T>::value || is_std_array<T>::value) {
        Json a = Json::array();
        for (const auto& item : v) a.push_back(to_json(item, ctx));
        return a;
    } else if constexpr (is_reflected_v<T>) {
        Json o = Json::object();
        for_each_field<T>([&](const auto& f) { o[f.name] = to_json(v.*(f.member), ctx); });
        return o;
    } else {
        static_assert(sizeof(T) == 0, "type is not serializable: ASTRAL_REFLECT it");
    }
}

template <class T> bool from_json(const Json& j, T& v, const LoadContext& ctx) {
    if constexpr (std::is_same_v<T, Entity>) {
        if (j.is_null()) {
            v = {};
            return true;
        }
        if (ctx.resolve && ctx.resolve(j, v)) return true;
        ctx.error("reference " + j.dump() + " names no entity in this document");
        v = {};
        return false;
    } else if constexpr (is_handle<T>::value) {
        v = {};
        return true;
    } else if constexpr (std::is_same_v<T, bool>) {
        if (!j.is_bool()) return ctx.error("expected true/false"), false;
        v = j.as_bool();
        return true;
    } else if constexpr (std::is_enum_v<T> || std::is_arithmetic_v<T>) {
        if constexpr (is_reflected_enum_v<T>) {
            if (j.is_string()) {
                const auto value = enum_from_name<T>(j.as_string());
                if (!value) return ctx.error("unknown enumerator \"" + j.as_string() + "\""), false;
                v = *value;
                return true;
            }
        }
        if (!j.is_number()) return ctx.error("expected a number"), false;
        if constexpr (std::is_enum_v<T>) {
            v = static_cast<T>(static_cast<std::underlying_type_t<T>>(j.as_number()));
        } else {
            v = static_cast<T>(j.as_number());
        }
        return true;
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (!j.is_string()) return ctx.error("expected a string"), false;
        v = j.as_string();
        return true;
    } else if constexpr (std::is_same_v<T, Vec2>) {
        return detail::read_floats(j, &v.x, 2, ctx);
    } else if constexpr (std::is_same_v<T, Vec3>) {
        return detail::read_floats(j, &v.x, 3, ctx);
    } else if constexpr (std::is_same_v<T, Vec4> || std::is_same_v<T, Quat>) {
        return detail::read_floats(j, &v.x, 4, ctx);
    } else if constexpr (std::is_same_v<T, Mat4>) {
        return detail::read_floats(j, &v[0].x, 16, ctx);
    } else if constexpr (is_optional<T>::value) {
        if (j.is_null()) {
            v.reset();
            return true;
        }
        typename T::value_type inner{};
        if (!from_json(j, inner, ctx)) return false;
        v = std::move(inner);
        return true;
    } else if constexpr (is_vector<T>::value) {
        if (!j.is_array()) return ctx.error("expected an array"), false;
        T out;
        out.reserve(j.size());
        bool ok = true;
        for (const Json& item : j.as_array()) {
            typename T::value_type value{};
            ok = from_json(item, value, ctx) && ok;
            out.push_back(std::move(value));
        }
        v = std::move(out);
        return ok;
    } else if constexpr (is_std_array<T>::value) {
        if (!j.is_array() || j.size() != v.size()) return ctx.error("expected an array of " + std::to_string(v.size())), false;
        bool ok = true;
        for (std::size_t i = 0; i < v.size(); ++i) ok = from_json(j[i], v[i], ctx) && ok;
        return ok;
    } else if constexpr (is_reflected_v<T>) {
        if (!j.is_object()) return ctx.error("expected an object"), false;
        bool ok = true;
        for_each_field<T>([&](const auto& f) {
            const Json* member = j.find(f.name);
            if (!member) return; // missing: keep the default (older files)
            LoadContext inner = ctx;
            inner.path = ctx.path + "." + std::string(f.name);
            auto value = v.*(f.member);
            if (from_json(*member, value, inner)) v.*(f.member) = std::move(value);
            else ok = false;
        });
        return ok;
    } else {
        static_assert(sizeof(T) == 0, "type is not serializable: ASTRAL_REFLECT it");
    }
}

/// Components that can be saved and loaded, by stable name.
class TypeRegistry {
public:
    struct Entry {
        std::string_view name;
        std::uint64_t id = 0; ///< stable_hash(name)
        ComponentId component = 0;
        std::function<bool(World&, Entity)> has;
        std::function<Json(World&, Entity, const SaveContext&)> save;
        std::function<void(World&, Entity, const Json&, const LoadContext&)> load;
        std::function<void(World&, Entity)> remove;
        /// Reads `j` as T and writes it back: fills missing fields with defaults, rounds numbers to the
        /// field's type, and lets the two contexts rewrite entity references. Errors go to `in`.
        std::function<Json(const Json& j, const LoadContext& in, const SaveContext& out)> transcode;
        /// Memory layout of the fields (for scripts and tools); declaration order.
        std::vector<FieldInfo> fields;
        /// Script-defined (add_dynamic): plain data, no C++ type behind it.
        bool dynamic = false;
    };

    /// T must be ASTRAL_REFLECT'ed. Registering again is a no-op.
    template <class T> void add() {
        static_assert(is_reflected_v<T>, "ASTRAL_REFLECT the component before registering it");
        if (find(Reflect<T>::name)) return;
        entries_.push_back(Entry{
            Reflect<T>::name, Reflect<T>::id, component_id<T>(),
            [](World& w, Entity e) { return w.has<T>(e); },
            [](World& w, Entity e, const SaveContext& ctx) { return to_json(*w.get<const T>(e), ctx); },
            [](World& w, Entity e, const Json& j, const LoadContext& ctx) {
                T value{};
                from_json(j, value, ctx); // partial values are kept: errors were reported
                w.add(e, std::move(value));
            },
            [](World& w, Entity e) { w.remove<T>(e); },
            [](const Json& j, const LoadContext& in, const SaveContext& out) {
                T value{};
                from_json(j, value, in);
                return to_json(value, out);
            },
            describe_fields<T>(), false});
    }

    /// Defines a component from a schema at run time (plain data: Bool, Int, UInt, Float, Vec2..Quat,
    /// Entity; at most 256 bytes). It behaves like a reflected C++ one: scenes, saves and the inspector
    /// know it. Null with `*error` set if the name is taken or the schema is invalid. The pointer stays
    /// valid until the next add; keep the Entry's index (entries()) for longer.
    const Entry* add_dynamic(std::string_view name, std::vector<FieldDef> fields, std::string* error = nullptr);

    [[nodiscard]] const Entry* find(std::string_view name) const noexcept;
    [[nodiscard]] std::span<const Entry> entries() const noexcept { return entries_; }

    /// Entities that have T are never saved (generated at runtime, e.g. the pieces of a spawned model).
    template <class T> void exclude() {
        excluded_.push_back([](World& w, Entity e) { return w.has<T>(e); });
    }
    [[nodiscard]] bool is_excluded(World& world, Entity e) const {
        for (auto check : excluded_) {
            if (check(world, e)) return true;
        }
        return false;
    }

private:
    std::vector<Entry> entries_;
    std::deque<std::string> owned_names_; // names of script-defined components
    std::vector<bool (*)(World&, Entity)> excluded_;
};

/// Registers the world module's own components (Name, Transform, Parent).
void register_world_types(TypeRegistry& types);

struct LoadResult {
    std::vector<Entity> entities;     ///< In document order.
    std::vector<std::string> errors;  ///< Empty when everything loaded cleanly.
};

/// Contexts for live entities: references are "#index.generation" (they survive a round trip while the
/// entity lives). Used by tools, scripts and event payloads. They refer to `world`: use them at once.
[[nodiscard]] SaveContext live_save_context(World& world);
[[nodiscard]] LoadContext live_load_context(World& world, std::vector<std::string>* errors, std::string path = {});

/// One component of a live entity as JSON, for tools (inspector, console, network). Entity references
/// are written as "#index.generation" so they survive a round trip. Null if the entity lacks it.
[[nodiscard]] Json component_to_json(World& world, const TypeRegistry::Entry& type, Entity entity);
/// Adds or replaces the component from JSON (missing fields keep their defaults; "#i.g" references
/// resolve to live entities). Returns the problems found (empty = clean).
std::vector<std::string> component_from_json(World& world, const TypeRegistry::Entry& type, Entity entity, const Json& value);

/// Every entity that has at least one registered component, in a stable order.
[[nodiscard]] Json save_world(World& world, const TypeRegistry& types);
/// Appends the document's entities to `world`. Refuses (creating nothing) on a wrong format/version.
LoadResult load_world(World& world, const TypeRegistry& types, const Json& document);

} // namespace astral
