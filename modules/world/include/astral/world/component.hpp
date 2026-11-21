#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

namespace astral {

/// Identifier of a component type within this program: the address of a per-type tag, so
/// distinct types never collide — even same-named types in anonymous namespaces of different
/// .cpp files. No registration and no global counter. Not stable across runs: use
/// component_name_hash() for anything persisted (save files, network).
using ComponentId = std::uintptr_t;

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

namespace detail {
template <class T> inline constexpr char type_tag = 0;
} // namespace detail

template <class T> ComponentId component_id() noexcept {
    return reinterpret_cast<ComponentId>(&detail::type_tag<std::remove_cvref_t<T>>);
}

/// Hash of the type's printed name: stable between runs of the same build, but NOT unique
/// for same-named types in anonymous namespaces. For serialization keys only.
template <class T> constexpr std::uint64_t component_name_hash() noexcept {
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
    static const ComponentInfo info{
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
