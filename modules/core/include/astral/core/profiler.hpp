#pragma once
// CPU frame profiler: nested named scopes on the main thread, plus a frame-time history.
//
//     Profiler profiler;
//     profiler.begin_frame();
//     { ASTRAL_PROFILE_SCOPE(&profiler, "physics"); step(); }
//     profiler.end_frame();
//     for (const ProfileSample& s : profiler.last_frame()) { ... s.name, s.depth, s.ms ... }
//
// Not thread-safe: record from the thread that calls begin_frame(). Scope names must outlive the
// profiler (string literals, or strings owned by long-lived objects).

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace astral {

struct ProfileSample {
    const char* name = "";
    std::uint16_t depth = 0;  ///< 0 = top level.
    float start_ms = 0.0f;    ///< From the start of the frame.
    float ms = 0.0f;
};

class Profiler {
public:
    explicit Profiler(std::size_t history_frames = 240);

    void begin_frame();
    void end_frame();
    void begin(const char* name);
    void end();

    /// Scopes of the last completed frame, in the order they began.
    [[nodiscard]] std::span<const ProfileSample> last_frame() const noexcept { return last_; }
    [[nodiscard]] float last_frame_ms() const noexcept { return last_ms_; }
    /// Frame times (ms), oldest first.
    [[nodiscard]] std::vector<float> frame_times() const;
    /// Total time of every scope named `name` in the last frame (0 if absent).
    [[nodiscard]] float scope_ms(std::string_view name) const noexcept;

    void set_enabled(bool enabled) noexcept { enabled_ = enabled; }
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }

private:
    using Clock = std::chrono::steady_clock;
    [[nodiscard]] float since_frame(Clock::time_point t) const noexcept;

    bool enabled_ = true;
    bool in_frame_ = false;
    Clock::time_point frame_start_{};
    std::vector<ProfileSample> current_, last_;
    std::vector<std::size_t> open_;
    std::vector<float> history_;
    std::size_t history_next_ = 0;
    std::size_t history_count_ = 0;
    float last_ms_ = 0.0f;
};

/// Times the enclosing scope (does nothing with a null profiler).
class ProfileScope {
public:
    ProfileScope(Profiler* profiler, const char* name) : profiler_(profiler) {
        if (profiler_) profiler_->begin(name);
    }
    ~ProfileScope() {
        if (profiler_) profiler_->end();
    }
    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;

private:
    Profiler* profiler_;
};

/// Readable name of a C++ type from typeid(...).name() ("PlayerControl", "pong::Ball").
std::string demangle(const char* mangled);

} // namespace astral

#define ASTRAL_PROFILE_CAT2(a, b) a##b
#define ASTRAL_PROFILE_CAT(a, b) ASTRAL_PROFILE_CAT2(a, b)
/// Times the rest of the enclosing block: ASTRAL_PROFILE_SCOPE(ctx.profiler, "pathfinding");
#define ASTRAL_PROFILE_SCOPE(profiler, name) ::astral::ProfileScope ASTRAL_PROFILE_CAT(astral_profile_, __LINE__)(profiler, name)
