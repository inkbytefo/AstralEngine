#pragma once
// Compile-time reflection: a type lists its fields once, and serialization, editors, networking
// and tools read that list. No code generator, no RTTI.
//
//     struct Health { float current = 100; float max = 100; };
//     ASTRAL_REFLECT(Health, current, max)     // at global namespace scope, after the type
//
// The name given to the macro (as written, e.g. "game::Health") is the type's stable identity in
// files: it does not depend on the compiler and survives rebuilds. Rename a type = rename its key.

#include <cstddef>
#include <cstdint>
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

// --- macro machinery: apply ASTRAL_DETAIL_FIELD to up to 32 field names -----------------------
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
#define ASTRAL_DETAIL_COUNT(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19,   \
                            _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, N, ...)                \
    N
#define ASTRAL_DETAIL_NARGS(...)                                                                                     \
    ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_COUNT(__VA_ARGS__, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19,   \
                                             18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1))
#define ASTRAL_DETAIL_CAT2(a, b) a##b
#define ASTRAL_DETAIL_CAT(a, b) ASTRAL_DETAIL_CAT2(a, b)
#define ASTRAL_DETAIL_FIELDS(T, ...)                                                                                 \
    ASTRAL_DETAIL_EXPAND(ASTRAL_DETAIL_CAT(ASTRAL_DETAIL_F, ASTRAL_DETAIL_NARGS(__VA_ARGS__))(T, __VA_ARGS__))

/// Reflects `Type` with the listed fields (1..32). Use at global namespace scope.
#define ASTRAL_REFLECT(Type, ...)                                                                                    \
    template <> struct astral::Reflect<Type> {                                                                       \
        static constexpr bool reflected = true;                                                                      \
        static constexpr std::string_view name = #Type;                                                              \
        static constexpr std::uint64_t id = ::astral::stable_hash(#Type);                                            \
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
