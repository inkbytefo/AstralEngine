#pragma once
// Internal to Astral::App: deterministic wave-based ECS system scheduler (ADR-0032).

#include <astral/app/system.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace astral {

class Context;

/// Stores systems per frame phase, caches conflict-free parallel execution waves,
/// and safely defers system additions made re-entrantly during phase execution.
class SystemScheduler {
public:
    struct Wave {
        std::size_t begin = 0;
        std::size_t end = 0;
    };

    explicit SystemScheduler(bool parallel_enabled = true) noexcept : parallel_(parallel_enabled) {}
    ~SystemScheduler() { clear(); }
    SystemScheduler(const SystemScheduler&) = delete;
    SystemScheduler& operator=(const SystemScheduler&) = delete;

    void add(Phase phase, std::unique_ptr<System> system);
    void clear();

    void set_parallel(bool enabled) noexcept;
    [[nodiscard]] bool parallel() const noexcept { return parallel_; }

    void run(Phase phase, Context& ctx);

    [[nodiscard]] std::size_t wave_count(Phase phase);
    [[nodiscard]] std::size_t system_count(Phase phase) const noexcept;

private:
    struct PhaseBucket {
        std::vector<std::unique_ptr<System>> systems;
        std::vector<Wave> waves;
        bool dirty = true;
    };
    struct PendingSystem {
        Phase phase = Phase::Update;
        std::unique_ptr<System> system;
    };

    static void rebuild_waves(PhaseBucket& bucket, bool allow_parallel);
    static void run_serial_system(System& sys, Context& ctx, std::uint64_t producer);
    static void run_parallel_wave(std::span<const std::unique_ptr<System>> wave_systems, Context& ctx, std::size_t phase,
                                  std::size_t first_index);
    void flush_pending();

    std::array<PhaseBucket, 4> buckets_;
    std::vector<PendingSystem> pending_;
    int run_depth_ = 0;
    bool parallel_ = true;
};

} // namespace astral
