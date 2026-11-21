#include <astral/core/clock.hpp>

#include <algorithm>

namespace astral {

std::chrono::nanoseconds SteadyTimeSource::now() const {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
}

const ITimeSource& default_time_source() noexcept {
    static const SteadyTimeSource source;
    return source;
}

Clock::Clock(const ITimeSource& source) noexcept : source_(&source) {}

float Clock::tick() noexcept {
    const auto now = source_->now();
    if (!start_) {
        start_ = now;
        last_ = now;
        return 0.0f;
    }
    const std::chrono::duration<float> delta = now - last_;
    last_ = now;
    return std::min(delta.count(), max_delta_);
}

double Clock::elapsed() const noexcept {
    if (!start_) {
        return 0.0;
    }
    return std::chrono::duration<double>(last_ - *start_).count();
}

void Clock::set_max_delta(float seconds) noexcept { max_delta_ = seconds; }

} // namespace astral
