#include <astral/core/profiler.hpp>

#include <algorithm>
#include <cstring>

#if defined(__GNUC__) || defined(__clang__)
#include <cstdlib>
#include <cxxabi.h>
#endif

namespace astral {

Profiler::Profiler(std::size_t history_frames) : history_(std::max<std::size_t>(history_frames, 1), 0.0f) {}

float Profiler::since_frame(Clock::time_point t) const noexcept {
    return std::chrono::duration<float, std::milli>(t - frame_start_).count();
}

void Profiler::begin_frame() {
    if (!enabled_) return;
    if (in_frame_) end_frame();
    in_frame_ = true;
    current_.clear();
    open_.clear();
    frame_start_ = Clock::now();
}

void Profiler::end_frame() {
    if (!in_frame_) return;
    const auto now = Clock::now();
    while (!open_.empty()) end(); // unbalanced scopes are closed at the frame's end
    in_frame_ = false;
    last_ms_ = since_frame(now);
    std::swap(last_, current_);
    history_[history_next_] = last_ms_;
    history_next_ = (history_next_ + 1) % history_.size();
    history_count_ = std::min(history_count_ + 1, history_.size());
}

void Profiler::begin(const char* name) {
    if (!in_frame_) return;
    current_.push_back({name ? name : "", static_cast<std::uint16_t>(open_.size()), since_frame(Clock::now()), 0.0f});
    open_.push_back(current_.size() - 1);
}

void Profiler::end() {
    if (!in_frame_ || open_.empty()) return;
    ProfileSample& s = current_[open_.back()];
    open_.pop_back();
    s.ms = since_frame(Clock::now()) - s.start_ms;
}

std::vector<float> Profiler::frame_times() const {
    std::vector<float> out;
    out.reserve(history_count_);
    const std::size_t start = (history_next_ + history_.size() - history_count_) % history_.size();
    for (std::size_t i = 0; i < history_count_; ++i) out.push_back(history_[(start + i) % history_.size()]);
    return out;
}

float Profiler::scope_ms(std::string_view name) const noexcept {
    float total = 0.0f;
    for (const ProfileSample& s : last_) {
        if (name == s.name) total += s.ms;
    }
    return total;
}

std::string demangle(const char* mangled) {
    std::string name = mangled ? mangled : "";
#if defined(__GNUC__) || defined(__clang__)
    int status = 0;
    if (char* readable = abi::__cxa_demangle(mangled, nullptr, nullptr, &status)) {
        if (status == 0) name = readable;
        std::free(readable);
    }
#endif
    for (const char* prefix : {"class ", "struct "}) { // MSVC style
        if (name.rfind(prefix, 0) == 0) name.erase(0, std::strlen(prefix));
    }
    // Anonymous namespaces add noise: "(anonymous namespace)::PlayerControl" -> "PlayerControl".
    for (const char* noise : {"(anonymous namespace)::", "`anonymous namespace'::"}) {
        for (std::size_t at; (at = name.find(noise)) != std::string::npos;) name.erase(at, std::strlen(noise));
    }
    return name;
}

} // namespace astral
