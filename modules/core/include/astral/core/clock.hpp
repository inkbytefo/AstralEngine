#pragma once

#include <chrono>
#include <optional>

namespace astral {

/// Abstract monotonic time source. Inject a fake one in tests or for replays.
class ITimeSource {
public:
    virtual ~ITimeSource() = default;
    [[nodiscard]] virtual std::chrono::nanoseconds now() const = 0;
};

/// std::chrono::steady_clock — the default source.
class SteadyTimeSource final : public ITimeSource {
public:
    [[nodiscard]] std::chrono::nanoseconds now() const override;
};

/// Shared stateless steady source used when no source is given.
const ITimeSource& default_time_source() noexcept;

/// Frame clock. Call tick() once per frame; it returns the frame delta in seconds,
/// clamped to max_delta so a breakpoint or hitch does not explode the simulation.
class Clock {
public:
    explicit Clock(const ITimeSource& source = default_time_source()) noexcept;

    float tick() noexcept;
    [[nodiscard]] double elapsed() const noexcept; ///< Unclamped seconds since the first tick.
    void set_max_delta(float seconds) noexcept;

private:
    const ITimeSource* source_;
    std::optional<std::chrono::nanoseconds> start_;
    std::chrono::nanoseconds last_{0};
    float max_delta_ = 0.25f;
};

} // namespace astral
