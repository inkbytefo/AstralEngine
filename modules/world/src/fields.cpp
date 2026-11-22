#include <astral/world/fields.hpp>
#include <astral/world/serialize.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace astral {

std::string_view field_kind_name(FieldKind kind) noexcept {
    switch (kind) {
    case FieldKind::Bool: return "bool";
    case FieldKind::Int: return "i32";
    case FieldKind::UInt: return "u32";
    case FieldKind::Float: return "f32";
    case FieldKind::Vec2: return "vec2";
    case FieldKind::Vec3: return "vec3";
    case FieldKind::Vec4: return "vec4";
    case FieldKind::Quat: return "quat";
    case FieldKind::EntityRef: return "entity";
    case FieldKind::Enum: return "enum";
    case FieldKind::String: return "string";
    case FieldKind::Other: break;
    }
    return "other";
}

std::optional<FieldKind> field_kind_from_name(std::string_view name) noexcept {
    for (FieldKind k : {FieldKind::Bool, FieldKind::Int, FieldKind::UInt, FieldKind::Float, FieldKind::Vec2,
                        FieldKind::Vec3, FieldKind::Vec4, FieldKind::Quat, FieldKind::EntityRef}) {
        if (field_kind_name(k) == name) return k;
    }
    return std::nullopt;
}

namespace {
template <class T> T load(const void* p) noexcept {
    T v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
template <class T> void store(void* p, T v) noexcept { std::memcpy(p, &v, sizeof v); }

template <class T> void store_clamped(void* p, double value) noexcept {
    if (std::isnan(value)) value = 0;
    const double lo = static_cast<double>(std::numeric_limits<T>::lowest());
    const double hi = static_cast<double>(std::numeric_limits<T>::max());
    store<T>(p, static_cast<T>(std::clamp(std::round(value), lo, hi)));
}
} // namespace

double read_number(const FieldInfo& f, const void* base) noexcept {
    const auto* p = static_cast<const std::byte*>(base) + f.offset;
    switch (f.kind) {
    case FieldKind::Bool: return load<std::uint8_t>(p) != 0 ? 1.0 : 0.0;
    case FieldKind::Float: return f.size == 8 ? load<double>(p) : static_cast<double>(load<float>(p));
    case FieldKind::Int:
    case FieldKind::Enum:
    case FieldKind::UInt: {
        const bool sign = f.is_signed && f.kind != FieldKind::UInt;
        switch (f.size) {
        case 1: return sign ? load<std::int8_t>(p) : load<std::uint8_t>(p);
        case 2: return sign ? load<std::int16_t>(p) : load<std::uint16_t>(p);
        case 8: return sign ? static_cast<double>(load<std::int64_t>(p)) : static_cast<double>(load<std::uint64_t>(p));
        default: return sign ? load<std::int32_t>(p) : static_cast<double>(load<std::uint32_t>(p));
        }
    }
    default: return 0.0;
    }
}

void write_number(const FieldInfo& f, void* base, double value) noexcept {
    auto* p = static_cast<std::byte*>(base) + f.offset;
    switch (f.kind) {
    case FieldKind::Bool: store<std::uint8_t>(p, value != 0.0 ? 1 : 0); return;
    case FieldKind::Float:
        if (f.size == 8) store<double>(p, value);
        else store<float>(p, static_cast<float>(value));
        return;
    case FieldKind::Int:
    case FieldKind::Enum:
    case FieldKind::UInt: {
        const bool sign = f.is_signed && f.kind != FieldKind::UInt;
        switch (f.size) {
        case 1: sign ? store_clamped<std::int8_t>(p, value) : store_clamped<std::uint8_t>(p, value); return;
        case 2: sign ? store_clamped<std::int16_t>(p, value) : store_clamped<std::uint16_t>(p, value); return;
        case 8: sign ? store_clamped<std::int64_t>(p, value) : store_clamped<std::uint64_t>(p, value); return;
        default: sign ? store_clamped<std::int32_t>(p, value) : store_clamped<std::uint32_t>(p, value); return;
        }
    }
    default: return;
    }
}

const FieldInfo* find_field(const std::vector<FieldInfo>& fields, std::string_view name) noexcept {
    for (const FieldInfo& f : fields) {
        if (f.name == name) return &f;
    }
    return nullptr;
}

Json field_to_json(const FieldInfo& f, const void* base, const SaveContext& ctx) {
    const auto* p = static_cast<const std::byte*>(base) + f.offset;
    switch (f.kind) {
    case FieldKind::Bool: return read_number(f, base) != 0.0;
    case FieldKind::Float: return f.size == 8 ? Json(load<double>(p)) : Json(detail::float_value(load<float>(p)));
    case FieldKind::Int:
    case FieldKind::UInt: return read_number(f, base);
    case FieldKind::Enum: {
        const auto value = static_cast<std::int64_t>(read_number(f, base));
        for (const auto& [name, v] : f.enumerators) {
            if (v == value) return name;
        }
        return static_cast<double>(value);
    }
    case FieldKind::Vec2: return detail::floats(reinterpret_cast<const float*>(p), 2);
    case FieldKind::Vec3: return detail::floats(reinterpret_cast<const float*>(p), 3);
    case FieldKind::Vec4:
    case FieldKind::Quat: return detail::floats(reinterpret_cast<const float*>(p), 4);
    case FieldKind::EntityRef: return ctx.entity(load<Entity>(p));
    default: return nullptr;
    }
}

bool field_from_json(const FieldInfo& f, void* base, const Json& j, const LoadContext& ctx) {
    auto* p = static_cast<std::byte*>(base) + f.offset;
    switch (f.kind) {
    case FieldKind::Bool:
        if (!j.is_bool()) return ctx.error("expected true/false"), false;
        write_number(f, base, j.as_bool() ? 1.0 : 0.0);
        return true;
    case FieldKind::Float:
    case FieldKind::Int:
    case FieldKind::UInt:
        if (!j.is_number()) return ctx.error("expected a number"), false;
        write_number(f, base, j.as_number());
        return true;
    case FieldKind::Enum:
        if (j.is_string()) {
            for (const auto& [name, v] : f.enumerators) {
                if (name == j.as_string()) return write_number(f, base, static_cast<double>(v)), true;
            }
            return ctx.error("unknown enumerator \"" + j.as_string() + "\""), false;
        }
        if (!j.is_number()) return ctx.error("expected an enumerator name"), false;
        write_number(f, base, j.as_number());
        return true;
    case FieldKind::Vec2: return detail::read_floats(j, reinterpret_cast<float*>(p), 2, ctx);
    case FieldKind::Vec3: return detail::read_floats(j, reinterpret_cast<float*>(p), 3, ctx);
    case FieldKind::Vec4:
    case FieldKind::Quat: return detail::read_floats(j, reinterpret_cast<float*>(p), 4, ctx);
    case FieldKind::EntityRef: {
        Entity e{};
        if (!j.is_null()) {
            if (!(ctx.resolve && ctx.resolve(j, e))) return ctx.error("reference " + j.dump() + " names no entity"), false;
        }
        store(p, e);
        return true;
    }
    default: return ctx.error("this field cannot be read from JSON"), false;
    }
}

} // namespace astral
