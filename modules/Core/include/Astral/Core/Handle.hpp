#pragma once

#include "Astral/Core/Assert.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace Astral {

// Typed generational handle: 32-bit slot index + 32-bit generation.
// A handle stays invalid after its object is destroyed, even if the slot is reused.
// `Tag` only makes different handle kinds incompatible at compile time.
template <typename Tag>
struct Handle {
    uint32_t index = 0;
    uint32_t generation = 0; // 0 = null

    constexpr bool IsValid() const { return generation != 0; }
    constexpr explicit operator bool() const { return IsValid(); }
    constexpr uint64_t Raw() const { return (uint64_t(generation) << 32) | index; }
    static constexpr Handle FromRaw(uint64_t raw) { return {uint32_t(raw), uint32_t(raw >> 32)}; }

    friend constexpr bool operator==(Handle, Handle) = default;
};

// Dense storage addressed by handles. Objects live in a vector of slots; freed slots
// are recycled and their generation is bumped, which invalidates old handles.
template <typename Tag, typename T>
class HandlePool {
public:
    using HandleType = Handle<Tag>;

    template <typename... Args>
    HandleType Create(Args&&... args) {
        uint32_t index;
        if (!m_FreeList.empty()) {
            index = m_FreeList.back();
            m_FreeList.pop_back();
        } else {
            index = static_cast<uint32_t>(m_Slots.size());
            m_Slots.emplace_back();
        }
        Slot& slot = m_Slots[index];
        slot.value = T{std::forward<Args>(args)...};
        slot.alive = true;
        ++m_AliveCount;
        return {index, slot.generation};
    }

    bool Destroy(HandleType handle) {
        if (!Contains(handle)) return false;
        Slot& slot = m_Slots[handle.index];
        slot.alive = false;
        slot.value = T{};
        if (++slot.generation == 0) slot.generation = 1; // never hand out generation 0
        m_FreeList.push_back(handle.index);
        --m_AliveCount;
        return true;
    }

    bool Contains(HandleType handle) const {
        return handle.IsValid() && handle.index < m_Slots.size() &&
               m_Slots[handle.index].alive && m_Slots[handle.index].generation == handle.generation;
    }

    T& Get(HandleType handle) {
        ASTRAL_ASSERT(Contains(handle), "stale or invalid handle");
        return m_Slots[handle.index].value;
    }
    const T& Get(HandleType handle) const {
        ASTRAL_ASSERT(Contains(handle), "stale or invalid handle");
        return m_Slots[handle.index].value;
    }
    T* TryGet(HandleType handle) { return Contains(handle) ? &m_Slots[handle.index].value : nullptr; }
    const T* TryGet(HandleType handle) const { return Contains(handle) ? &m_Slots[handle.index].value : nullptr; }

    uint32_t AliveCount() const { return m_AliveCount; }
    uint32_t SlotCount() const { return static_cast<uint32_t>(m_Slots.size()); }

    // Visits every live object: fn(HandleType, T&).
    template <typename Fn>
    void ForEach(Fn&& fn) {
        for (uint32_t i = 0; i < m_Slots.size(); ++i)
            if (m_Slots[i].alive) fn(HandleType{i, m_Slots[i].generation}, m_Slots[i].value);
    }
    template <typename Fn>
    void ForEach(Fn&& fn) const {
        for (uint32_t i = 0; i < m_Slots.size(); ++i)
            if (m_Slots[i].alive) fn(HandleType{i, m_Slots[i].generation}, m_Slots[i].value);
    }

private:
    struct Slot {
        T value{};
        uint32_t generation = 1;
        bool alive = false;
    };
    std::vector<Slot> m_Slots;
    std::vector<uint32_t> m_FreeList;
    uint32_t m_AliveCount = 0;
};

} // namespace Astral

template <typename Tag>
struct std::hash<Astral::Handle<Tag>> {
    size_t operator()(Astral::Handle<Tag> h) const noexcept { return std::hash<uint64_t>{}(h.Raw()); }
};
