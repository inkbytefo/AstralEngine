#pragma once
// Which system is producing deferred commands on this thread (ADR-0043). Internal to app: the
// scheduler sets it around System::update; command queues read it to order commands the same way
// whether systems ran in parallel or one after another.

#include <cstddef>
#include <cstdint>

namespace astral::detail {

/// 0 outside any system (main-thread code: frame loop, scripts, tools). Inside a system:
/// ((phase + 1) << 32) | (index in the phase + 1), i.e. registration order.
[[nodiscard]] std::uint64_t current_command_producer() noexcept;
[[nodiscard]] constexpr std::uint64_t command_producer_rank(std::size_t phase, std::size_t index) noexcept {
    return ((static_cast<std::uint64_t>(phase) + 1) << 32) | (static_cast<std::uint64_t>(index) + 1);
}

/// Sets the current producer for this thread and restores the previous one on destruction.
class ScopedCommandProducer {
public:
    explicit ScopedCommandProducer(std::uint64_t rank) noexcept;
    ~ScopedCommandProducer();
    ScopedCommandProducer(const ScopedCommandProducer&) = delete;
    ScopedCommandProducer& operator=(const ScopedCommandProducer&) = delete;

private:
    std::uint64_t previous_;
};

} // namespace astral::detail
