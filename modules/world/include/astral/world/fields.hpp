#pragma once
// Field tables: what a component or event looks like in memory, discovered at run time. Built from
// ASTRAL_REFLECT for C++ types (describe_fields<T>) or from a schema for script-defined components.
// Scripts, inspectors and network code read and write fields through these tables.

#include <astral/core/json.hpp>
#include <astral/core/math.hpp>
#include <astral/core/reflect.hpp>
#include <astral/world/world.hpp>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace astral {

struct SaveContext;
struct LoadContext;

/// How a field's bytes are read. Int/UInt/Float/Enum/Bool convert to a number; Vec*/Quat are floats in
/// x, y, z, w order; Entity is an astral::Entity; String is a std::string (C++ types only);
/// Other is a type scripts cannot touch (nested structs, arrays, handles).
enum class FieldKind : std::uint8_t { Bool, Int, UInt, Float, Vec2, Vec3, Vec4, Quat, EntityRef, Enum, String, Other };

[[nodiscard]] std::string_view field_kind_name(FieldKind kind) noexcept;
/// Name -> kind ("f32", "i32", "u32", "bool", "vec2", "vec3", "vec4", "quat", "entity").
[[nodiscard]] std::optional<FieldKind> field_kind_from_name(std::string_view name) noexcept;

struct FieldInfo {
    std::string name;
    FieldKind kind = FieldKind::Other;
    std::uint32_t offset = 0; ///< Bytes from the start of the component or event.
    std::uint32_t size = 0;   ///< Bytes of the field (Int/UInt/Float/Enum: 1, 2, 4 or 8).
    bool is_signed = true;    ///< Int and Enum.
    std::vector<std::pair<std::string, std::int64_t>> enumerators; ///< Enum only.
};

/// A field of a script-defined component.
struct FieldDef {
    std::string name;
    FieldKind kind = FieldKind::Float;
    Json default_value = nullptr; ///< Null: zero / false / no entity.
};

/// Numeric value of a Bool/Int/UInt/Float/Enum field. `base` points at the component or event.
[[nodiscard]] double read_number(const FieldInfo& field, const void* base) noexcept;
/// Writes a number into a Bool/Int/UInt/Float/Enum field (rounded and clamped to the field's type).
void write_number(const FieldInfo& field, void* base, double value) noexcept;
[[nodiscard]] const FieldInfo* find_field(const std::vector<FieldInfo>& fields, std::string_view name) noexcept;

/// A field as JSON / from JSON, straight from memory (strings and Other are not handled here).
[[nodiscard]] Json field_to_json(const FieldInfo& field, const void* base, const SaveContext& ctx);
bool field_from_json(const FieldInfo& field, void* base, const Json& value, const LoadContext& ctx);

namespace detail {

template <class F> constexpr FieldKind kind_of() noexcept {
    using T = std::remove_cvref_t<F>;
    if constexpr (std::is_same_v<T, bool>) return FieldKind::Bool;
    else if constexpr (std::is_enum_v<T>) return is_reflected_enum_v<T> ? FieldKind::Enum : FieldKind::Int;
    else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) return FieldKind::Float;
    else if constexpr (std::is_integral_v<T>) return std::is_signed_v<T> ? FieldKind::Int : FieldKind::UInt;
    else if constexpr (std::is_same_v<T, Vec2>) return FieldKind::Vec2;
    else if constexpr (std::is_same_v<T, Vec3>) return FieldKind::Vec3;
    else if constexpr (std::is_same_v<T, Vec4>) return FieldKind::Vec4;
    else if constexpr (std::is_same_v<T, Quat>) return FieldKind::Quat;
    else if constexpr (std::is_same_v<T, Entity>) return FieldKind::EntityRef;
    else if constexpr (std::is_same_v<T, std::string>) return FieldKind::String;
    else return FieldKind::Other;
}

template <class F> void add_enumerators(FieldInfo& info) {
    if constexpr (std::is_enum_v<F> && is_reflected_enum_v<F>) {
        constexpr auto values = ReflectEnum<F>::values();
        for (std::size_t i = 0; i < values.size(); ++i) {
            info.enumerators.emplace_back(std::string(nth_name(ReflectEnum<F>::names, i)),
                                          static_cast<std::int64_t>(values[i]));
        }
    }
}

} // namespace detail

/// The field table of a reflected type (declaration order).
template <class T> [[nodiscard]] std::vector<FieldInfo> describe_fields() {
    std::vector<FieldInfo> out;
    const T probe{};
    for_each_field<T>([&](const auto& f) {
        using F = std::remove_cvref_t<typename std::remove_cvref_t<decltype(f)>::type>;
        FieldInfo info;
        info.name = std::string(f.name);
        info.kind = detail::kind_of<F>();
        info.offset = static_cast<std::uint32_t>(reinterpret_cast<const char*>(&(probe.*f.member)) -
                                                 reinterpret_cast<const char*>(&probe));
        info.size = static_cast<std::uint32_t>(sizeof(F));
        if constexpr (std::is_enum_v<F>) info.is_signed = std::is_signed_v<std::underlying_type_t<F>>;
        else if constexpr (std::is_integral_v<F>) info.is_signed = std::is_signed_v<F>;
        detail::add_enumerators<F>(info);
        out.push_back(std::move(info));
    });
    return out;
}

} // namespace astral
