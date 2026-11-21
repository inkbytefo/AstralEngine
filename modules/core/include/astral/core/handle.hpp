#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace astral {

/// Typed, generational reference to an object owned by a HandlePool.
/// Copyable and trivially cheap; never dangles — a stale handle simply stops resolving.
template <class Tag> struct Handle {
    std::uint32_t index = 0;
    std::uint32_t generation = 0; ///< 0 means "never valid".

    [[nodiscard]] constexpr bool valid() const noexcept { return generation != 0; }
    constexpr bool operator==(const Handle&) const noexcept = default;
};

/// Dense slot storage addressed by Handle<T>. Freed slots are reused; each reuse
/// bumps the slot's generation so outstanding handles to the old object fail to resolve.
/// Not thread-safe: synchronize externally if shared.
/// Pointers returned by get() are invalidated by a later create() (storage may grow).
/// Generations are 32-bit: a slot reused 2^32-1 times wraps (practically never).
template <class T> class HandlePool {
public:
    using HandleType = Handle<T>;

    HandleType create(T value) {
        std::uint32_t index;
        if (!free_.empty()) {
            index = free_.back();
            free_.pop_back();
        } else {
            index = static_cast<std::uint32_t>(slots_.size());
            slots_.emplace_back();
        }
        Slot& slot = slots_[index];
        slot.value.emplace(std::move(value));
        ++size_;
        return {index, slot.generation};
    }

    [[nodiscard]] T* get(HandleType h) noexcept {
        Slot* slot = resolve(h);
        return slot ? &*slot->value : nullptr;
    }

    [[nodiscard]] const T* get(HandleType h) const noexcept {
        return const_cast<HandlePool*>(this)->get(h);
    }

    bool destroy(HandleType h) {
        Slot* slot = resolve(h);
        if (!slot) {
            return false;
        }
        slot->value.reset();
        // Advance the generation, skipping 0 which is reserved for "invalid".
        if (++slot->generation == 0) {
            slot->generation = 1;
        }
        free_.push_back(h.index);
        --size_;
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

private:
    struct Slot {
        std::optional<T> value;
        std::uint32_t generation = 1;
    };

    Slot* resolve(HandleType h) noexcept {
        if (!h.valid() || h.index >= slots_.size()) {
            return nullptr;
        }
        Slot& slot = slots_[h.index];
        if (slot.generation != h.generation || !slot.value) {
            return nullptr;
        }
        return &slot;
    }

    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_;
    std::size_t size_ = 0;
};

} // namespace astral
