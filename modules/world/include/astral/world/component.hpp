#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

namespace astral {

/// Stable identifier of a component type, computed at compile time from the type's name.
/// No registration and no global counter: the same type always gets the same id.
using ComponentId = std::uint64_t;

namespace detail {

template <class T> constexpr std::string_view type_signature() noexcept {
#if defined(_MSC_VER) && !defined(__clang__)
    return __FUNCSIG__;
#else
    return __PRETTY_FUNCTION__;
#endif
}

constexpr std::uint64_t fnv1a(std::string_view text) noexcept {
    std::uint64_t hash = 14695981039346656037ull;
    for (char c : text) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace detail

template <class T> constexpr ComponentId component_id() noexcept {
    return detail::fnv1a(detail::type_signature<std::remove_cvref_t<T>>());
}

/// Type-erased operations the world needs to store a component in raw column memory.
struct ComponentInfo {
    ComponentId id;
    std::size_t size;
    std::size_t align;
    void (*move_construct)(void* dst, void* src) noexcept;
    void (*move_assign)(void* dst, void* src) noexcept;
    void (*destroy)(void* ptr) noexcept;
};

template <class T> const ComponentInfo& component_info() noexcept {
    using C = std::remove_cvref_t<T>;
    static_assert(std::is_move_constructible_v<C>, "components must be move-constructible");
    static_assert(std::is_nothrow_move_constructible_v<C>,
                  "components must be nothrow move-constructible (they are relocated in memory)");
    static constexpr ComponentInfo info{
        component_id<C>(),
        sizeof(C),
        alignof(C),
        [](void* dst, void* src) noexcept { ::new (dst) C(std::move(*static_cast<C*>(src))); },
        [](void* dst, void* src) noexcept {
            if constexpr (std::is_move_assignable_v<C>) {
                *static_cast<C*>(dst) = std::move(*static_cast<C*>(src));
            } else {
                static_cast<C*>(dst)->~C();
                ::new (dst) C(std::move(*static_cast<C*>(src)));
            }
        },
        [](void* ptr) noexcept { static_cast<C*>(ptr)->~C(); },
    };
    return info;
}

} // namespace astral
