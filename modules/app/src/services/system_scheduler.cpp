#include "system_scheduler.hpp"

#include "command_producer.hpp"

#include <astral/app/app.hpp>
#include <astral/core/jobs.hpp>
#include <astral/core/profiler.hpp>

#include <algorithm>
#include <utility>

namespace astral {

namespace {

bool sorted_overlaps(std::span<const ComponentId> a, std::span<const ComponentId> b) noexcept {
    std::size_t ia = 0;
    std::size_t ib = 0;
    while (ia < a.size() && ib < b.size()) {
        if (a[ia] == b[ib]) return true;
        if (a[ia] < b[ib]) ++ia;
        else ++ib;
    }
    return false;
}

} // namespace

void SystemScheduler::run_serial_system(System& sys, Context& ctx, std::uint64_t producer) {
    ASTRAL_PROFILE_SCOPE(ctx.profiler, sys.name().c_str());
    detail::ScopedCommandProducer scope(producer);
    ctx.world.set_change_baseline(sys.last_run_);
    sys.update(ctx);
    sys.last_run_ = ctx.world.tick();
    ctx.world.advance_tick();
}

void SystemScheduler::run_parallel_wave(std::span<const std::unique_ptr<System>> wave_systems, Context& ctx, std::size_t phase,
                                        std::size_t first_index) {
    ASTRAL_PROFILE_SCOPE(ctx.profiler, "parallel_systems");
    const Tick base_tick = ctx.world.tick();
    JobCounter counter;
    for (std::size_t i = 0; i < wave_systems.size(); ++i) {
        System* sys = wave_systems[i].get();
        const Tick baseline = sys->last_run_;
        const Tick write_tick = base_tick + static_cast<Tick>(i);
        const std::uint64_t producer = detail::command_producer_rank(phase, first_index + i);
        ctx.jobs.submit(counter, [&ctx, sys, baseline, write_tick, producer] {
            World::ScopedTick scope(ctx.world, baseline, write_tick);
            detail::ScopedCommandProducer producer_scope(producer);
            sys->update(ctx);
        });
    }
    ctx.jobs.wait(counter);
    for (std::size_t i = 0; i < wave_systems.size(); ++i) {
        wave_systems[i]->last_run_ = base_tick + static_cast<Tick>(i);
    }
    ctx.world.set_tick(base_tick + static_cast<Tick>(wave_systems.size()));
}

void SystemAccess::add_read(ComponentId id) {
    if (std::binary_search(writes_.begin(), writes_.end(), id)) return;
    const auto it = std::lower_bound(reads_.begin(), reads_.end(), id);
    if (it == reads_.end() || *it != id) reads_.insert(it, id);
}

void SystemAccess::add_write(ComponentId id) {
    if (const auto it = std::lower_bound(reads_.begin(), reads_.end(), id); it != reads_.end() && *it == id) {
        reads_.erase(it);
    }
    const auto it = std::lower_bound(writes_.begin(), writes_.end(), id);
    if (it == writes_.end() || *it != id) writes_.insert(it, id);
}

bool SystemAccess::conflicts_with(const SystemAccess& other) const noexcept {
    if (exclusive() || other.exclusive()) return true;
    return sorted_overlaps(writes_, other.writes_) || sorted_overlaps(writes_, other.reads_) ||
           sorted_overlaps(reads_, other.writes_);
}

void SystemScheduler::add(Phase phase, std::unique_ptr<System> system) {
    if (!system) return;
    system->configure(system->access_);
    if (run_depth_ > 0) {
        pending_.push_back({phase, std::move(system)});
        return;
    }
    PhaseBucket& bucket = buckets_[static_cast<std::size_t>(phase)];
    bucket.systems.push_back(std::move(system));
    bucket.dirty = true;
}

void SystemScheduler::clear() {
    pending_.clear();
    for (PhaseBucket& bucket : buckets_) {
        bucket.systems.clear();
        bucket.waves.clear();
        bucket.dirty = true;
    }
}

void SystemScheduler::set_parallel(bool enabled) noexcept {
    if (parallel_ == enabled) return;
    parallel_ = enabled;
    for (PhaseBucket& bucket : buckets_) bucket.dirty = true;
}

void SystemScheduler::rebuild_waves(PhaseBucket& bucket, bool allow_parallel) {
    bucket.waves.clear();
    const auto& systems = bucket.systems;
    std::size_t i = 0;
    while (i < systems.size()) {
        std::size_t end = i + 1;
        if (allow_parallel && !systems[i]->access().exclusive()) {
            while (end < systems.size() && !systems[end]->access().exclusive()) {
                bool conflict = false;
                for (std::size_t k = i; k < end && !conflict; ++k) {
                    conflict = systems[end]->access().conflicts_with(systems[k]->access());
                }
                if (conflict) break;
                ++end;
            }
        }
        bucket.waves.push_back({i, end});
        i = end;
    }
    bucket.dirty = false;
}

void SystemScheduler::flush_pending() {
    for (PendingSystem& p : pending_) {
        PhaseBucket& bucket = buckets_[static_cast<std::size_t>(p.phase)];
        bucket.systems.push_back(std::move(p.system));
        bucket.dirty = true;
    }
    pending_.clear();
}

void SystemScheduler::run(Phase phase, Context& ctx) {
    PhaseBucket& bucket = buckets_[static_cast<std::size_t>(phase)];
    if (bucket.systems.empty()) return;
    if (bucket.dirty) rebuild_waves(bucket, parallel_);

    struct RunGuard {
        SystemScheduler& scheduler;
        explicit RunGuard(SystemScheduler& s) noexcept : scheduler(s) { ++scheduler.run_depth_; }
        ~RunGuard() {
            if (--scheduler.run_depth_ == 0 && !scheduler.pending_.empty()) scheduler.flush_pending();
        }
    } guard(*this);

    for (const Wave& w : bucket.waves) {
        if (w.end - w.begin == 1) {
            run_serial_system(*bucket.systems[w.begin], ctx, detail::command_producer_rank(static_cast<std::size_t>(phase), w.begin));
        } else {
            run_parallel_wave(std::span<const std::unique_ptr<System>>(bucket.systems).subspan(w.begin, w.end - w.begin), ctx,
                              static_cast<std::size_t>(phase), w.begin);
        }
    }
}

std::size_t SystemScheduler::wave_count(Phase phase) {
    PhaseBucket& bucket = buckets_[static_cast<std::size_t>(phase)];
    if (bucket.dirty) rebuild_waves(bucket, parallel_);
    return bucket.waves.size();
}

std::size_t SystemScheduler::system_count(Phase phase) const noexcept {
    return buckets_[static_cast<std::size_t>(phase)].systems.size();
}

} // namespace astral
