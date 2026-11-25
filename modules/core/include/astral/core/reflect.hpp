#pragma once
// Compile-time reflection: a type lists its fields once, and serialization, editors, networking
// and tools read that list. No code generator, no RTTI.
//
//     struct Health { float current = 100; float max = 100; };
//     ASTRAL_REFLECT(Health, current, max)     // at global namespace scope, after the type
//
// The name given to the macro (as written, e.g. "game::Health") is the type's stable identity in
// files: it does not depend on the compiler and survives rebuilds. Rename a type = rename its key.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace astral {

/// One reflected field: its name and member pointer.
template <class Class, class T> struct Field {
    std::string_view name;
    T Class::*member;
    using type = T;
};

/// Specialised by ASTRAL_REFLECT. `reflected` is false for every other type.
template <class T> struct Reflect {
    static constexpr bool reflected = false;
};

template <class T> inline constexpr bool is_reflected_v = Reflect<std::remove_cvref_t<T>>::reflected;

/// Calls fn(field) for every reflected field of T, in declaration order.
template <class T, class Fn> constexpr void for_each_field(Fn&& fn) {
    std::apply([&](const auto&... f) { (fn(f), ...); }, Reflect<std::remove_cvref_t<T>>::fields());
}

/// Specialised by ASTRAL_REFLECT_ENUM: enumerators by name (files say "Sphere", not 0).
template <class E> struct ReflectEnum {
    static constexpr bool reflected = false;
};
template <class E> inline constexpr bool is_reflected_enum_v = ReflectEnum<std::remove_cvref_t<E>>::reflected;

namespace detail {
/// The i-th name of a comma-separated list ("A, B, C"), trimmed.
constexpr std::string_view nth_name(std::string_view list, std::size_t i) noexcept {
    for (;; --i) {
        const std::size_t comma = list.find(',');
        std::string_view item = list.substr(0, comma);
        while (!item.empty() && (item.front() == ' ' || item.front() == '\n')) item.remove_prefix(1);
        while (!item.empty() && (item.back() == ' ' || item.back() == '\n')) item.remove_suffix(1);
        if (i == 0) return item;
        if (comma == std::string_view::npos) return {};
        list.remove_prefix(comma + 1);
    }
}
} // namespace detail

/// Name of a reflected enumerator; empty if `value` is not one of the listed enumerators.
template <class E> constexpr std::string_view enum_name(E value) noexcept {
    constexpr auto values = ReflectEnum<E>::values();
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (values[i] == value) return detail::nth_name(ReflectEnum<E>::names, i);
    }
    return {};
}

/// Enumerator by (exact) name.
template <class E> constexpr std::optional<E> enum_from_name(std::string_view name) noexcept {
    constexpr auto values = ReflectEnum<E>::values();
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (detail::nth_name(ReflectEnum<E>::names, i) == name) return values[i];
    }
    return std::nullopt;
}

/// Stable 64-bit id from a name (FNV-1a): the same everywhere, every build.
constexpr std::uint64_t stable_hash(std::string_view text) noexcept {
    std::uint64_t hash = 14695981039346656037ull;
    for (char c : text) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace astral

// --- macro machinery: apply ASTRAL_DETAIL_FIELD to up to 32 field names (counts up to 64) ------
#define ASTRAL_DETAIL_EXPAND(x) x
#define ASTRAL_DETAIL_FIELD(T, f) ::astral::Field<T, decltype(T::f)>{#f, &T::f}
#define ASTRAL_DETAIL_F1(T, a) ASTRAL_DETAIL_FIELD(T, a)
#define ASTRAL_DETAIL_F2(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F1(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F3(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F2(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F4(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F3(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F5(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F4(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F6(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F5(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F7(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F6(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F8(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F7(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F9(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F8(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F10(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F9(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F11(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F10(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F12(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F11(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F13(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F12(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F14(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F13(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F15(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F14(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F16(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F15(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F17(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F16(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F18(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F17(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F19(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F18(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F20(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F19(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F21(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F20(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F22(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F21(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F23(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F22(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F24(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F23(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F25(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F24(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F26(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F25(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F27(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F26(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F28(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F27(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F29(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F28(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F30(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F29(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F31(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F30(T, __VA_ARGS__))
#define ASTRAL_DETAIL_F32(T, a, ...) ASTRAL_DETAIL_FIELD(T, a), ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_F31(T, __VA_ARGS__))
#define ASTRAL_DETAIL_COUNT64(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, \
                              _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, \
                              _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, \
                              _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64, N, ...) \
    N
/// Number of arguments, 1..64.
#define ASTRAL_DETAIL_NARGS(...) \
    ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_COUNT64(__VA_ARGS__, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, \
        50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, \
        23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1))
/// Field-list arity: 1..32, or TOO_MANY (33..64), which expands to no fields so that only the
/// static_assert in ASTRAL_REFLECT reports the error.
#define ASTRAL_DETAIL_ARITY(...) \
    ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_COUNT64(__VA_ARGS__, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, \
        TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, \
        TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, \
        TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, TOO_MANY, 32, 31, 30, 29, 28, 27, 26, 25, 24, \
        23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1))
#define ASTRAL_DETAIL_FTOO_MANY(T, ...)
#define ASTRAL_DETAIL_CAT2(a, b) a##b
#define ASTRAL_DETAIL_CAT(a, b) ASTRAL_DETAIL_CAT2(a, b)
#define ASTRAL_DETAIL_FIELDS(T, ...)                                                                                 \
    ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_CAT(ASTRAL_DETAIL_F, ASTRAL_DETAIL_ARITY(__VA_ARGS__))(T, __VA_ARGS__))

/// Reflects `Type` with the listed fields (1..32; more is a compile error: split the component,
/// ADR-0030). Use at global namespace scope.
#define ASTRAL_REFLECT(Type, ...)                                                                                    \
    template <> struct astral::Reflect<Type> {                                                                       \
        static constexpr bool reflected = true;                                                                      \
        static constexpr std::string_view name = #Type;                                                              \
        static constexpr std::uint64_t id = ::astral::stable_hash(#Type);                                            \
        static_assert(ASTRAL_DETAIL_NARGS(__VA_ARGS__) <= 32,                                                        \
                      "ASTRAL_REFLECT supports at most 32 fields; split the component (ADR-0030)");                 \
        static constexpr auto fields() { return std::make_tuple(ASTRAL_DETAIL_FIELDS(Type, __VA_ARGS__)); }          \
    };

/// Reflects a type with no fields (a tag component).
#define ASTRAL_REFLECT_EMPTY(Type)                                                                                   \
    template <> struct astral::Reflect<Type> {                                                                       \
        static constexpr bool reflected = true;                                                                      \
        static constexpr std::string_view name = #Type;                                                              \
        static constexpr std::uint64_t id = ::astral::stable_hash(#Type);                                            \
        static constexpr auto fields() { return std::tuple<>{}; }                                                    \
    };

/// Lists an enum's enumerators so files can name them ("SmoothUnion" instead of 3). Any order and
/// explicit values are fine; unlisted values still save as numbers. Use at global namespace scope:
///     ASTRAL_REFLECT_ENUM(game::Team, Red, Blue)
#define ASTRAL_REFLECT_ENUM(Type, ...)                                                                               \
    template <> struct astral::ReflectEnum<Type> {                                                                   \
        static constexpr bool reflected = true;                                                                      \
        static constexpr std::string_view names = #__VA_ARGS__;                                                      \
        static constexpr auto values() {                                                                             \
            using enum Type;                                                                                         \
            return std::array<Type, ASTRAL_DETAIL_NARGS(__VA_ARGS__)>{__VA_ARGS__};                                  \
        }                                                                                                            \
    };
