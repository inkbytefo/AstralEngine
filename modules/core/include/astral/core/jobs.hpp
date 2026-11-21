#pragma once

#include <atomic>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>

namespace astral {

/// Tracks a group of jobs. Pass the same counter to submit() for every job in the
/// group, then wait() on it. Must outlive the jobs it tracks. Reusable after wait().
class JobCounter {
public:
    JobCounter() = default;
    JobCounter(const JobCounter&) = delete;
    JobCounter& operator=(const JobCounter&) = delete;

    [[nodiscard]] bool done() const noexcept { return pending_.load(std::memory_order_acquire) == 0; }

private:
    friend class JobSystem;
    std::atomic<std::size_t> pending_{0};
    std::mutex error_mutex_;
    std::exception_ptr error_; ///< First exception thrown by a job in this group.
};

/// Fixed pool of worker threads executing short jobs.
/// wait() helps: while its counter is pending it executes that counter's own queued
/// jobs itself, so waiting from inside a job — on its own children or on another
/// group — cannot deadlock the pool. A counter must only be used with one JobSystem.
class JobSystem {
public:
    using Job = std::function<void()>;
    using RangeJob = std::function<void(std::size_t begin, std::size_t end)>;

    /// `workers == 0` picks hardware_concurrency - 1 (at least 1).
    explicit JobSystem(unsigned workers = 0);
    ~JobSystem(); ///< Finishes every queued job, then joins the workers.

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    void submit(JobCounter& counter, Job job);
    /// Splits [0, count) into chunks of `batch` indices and submits one job per chunk.
    void parallel_for(JobCounter& counter, std::size_t count, std::size_t batch, RangeJob job);
    /// Blocks (while helping) until every job of `counter` finished. Rethrows the first
    /// exception any of them threw, then clears it so the counter can be reused.
    void wait(JobCounter& counter);

    [[nodiscard]] unsigned worker_count() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astral
