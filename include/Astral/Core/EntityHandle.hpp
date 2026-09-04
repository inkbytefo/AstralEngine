#pragma once

#include <cstdint>

namespace Astral {

using EntityHandle = std::uint64_t;
using EntityIndex = std::uint32_t;
using EntityGeneration = std::uint32_t;
using EntityID = EntityHandle;

constexpr EntityHandle NullEntityHandle = 0xFFFFFFFFFFFFFFFFull;
constexpr EntityHandle NullEntity = NullEntityHandle;

[[nodiscard]] constexpr inline EntityIndex GetEntityIndex(EntityHandle handle) noexcept {
    return static_cast<EntityIndex>(handle & 0xFFFFFFFFull);
}

[[nodiscard]] constexpr inline EntityGeneration GetEntityGeneration(EntityHandle handle) noexcept {
    return static_cast<EntityGeneration>((handle >> 32) & 0xFFFFFFFFull);
}

[[nodiscard]] constexpr inline EntityHandle MakeEntityHandle(EntityIndex index, EntityGeneration generation) noexcept {
    return (static_cast<EntityHandle>(generation) << 32) | static_cast<EntityHandle>(index);
}

} // namespace Astral
