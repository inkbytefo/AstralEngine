#include <astral/core/jobs.hpp>

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace astral {

namespace {
struct QueuedJob {
    JobSystem::Job fn;
    JobCounter* counter;
};
} // namespace

struct JobSystem::Impl {
    std::mutex mutex;
    std::condition_variable cv; // signalled on new work, on job completion and on stop
    std::deque<QueuedJob> queue;
    std::vector<std::thread> workers;
    bool stopping = false;

    std::optional<QueuedJob> try_pop_locked() {
        if (queue.empty()) {
            return std::nullopt;
        }
        QueuedJob job = std::move(queue.front());
        queue.pop_front();
        return job;
    }

    /// Pops the first queued job that belongs to `counter`. A waiter only helps its own
    /// group: running an unrelated job on top of a suspended one can make that job wait
    /// on the suspended one's group, which then never finishes (stack inversion).
    std::optional<QueuedJob> try_pop_for_locked(const JobCounter& counter) {
        for (auto it = queue.begin(); it != queue.end(); ++it) {
            if (it->counter == &counter) {
                QueuedJob job = std::move(*it);
                queue.erase(it);
                return job;
            }
        }
        return std::nullopt;
    }

    bool has_job_for_locked(const JobCounter& counter) const {
        for (const auto& job : queue) {
            if (job.counter == &counter) {
                return true;
            }
        }
        return false;
    }

    void execute(QueuedJob& job) {
        try {
            job.fn();
        } catch (...) {
            std::scoped_lock lock(job.counter->error_mutex_);
            if (!job.counter->error_) {
                job.counter->error_ = std::current_exception();
            }
        }
        job.counter->pending_.fetch_sub(1, std::memory_order_acq_rel);
        // Lock so a waiter can't miss the wake-up between its check and its sleep.
        std::scoped_lock lock(mutex);
        cv.notify_all();
    }

    void worker_loop() {
        for (;;) {
            std::optional<QueuedJob> job;
            {
                std::unique_lock lock(mutex);
                cv.wait(lock, [this] { return stopping || !queue.empty(); });
                job = try_pop_locked();
                if (!job) {
                    return; // stopping and nothing left
                }
            }
            execute(*job);
        }
    }
};

JobSystem::JobSystem(unsigned workers) : impl_(std::make_unique<Impl>()) {
    if (workers == 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        workers = hw > 1 ? hw - 1 : 1;
    }
    impl_->workers.reserve(workers);
    for (unsigned i = 0; i < workers; ++i) {
        impl_->workers.emplace_back([impl = impl_.get()] { impl->worker_loop(); });
    }
}

JobSystem::~JobSystem() {
    {
        std::scoped_lock lock(impl_->mutex);
        impl_->stopping = true;
    }
    impl_->cv.notify_all();
    for (auto& t : impl_->workers) {
        t.join();
    }
}

void JobSystem::submit(JobCounter& counter, Job job) {
    counter.pending_.fetch_add(1, std::memory_order_acq_rel);
    try {
        std::scoped_lock lock(impl_->mutex);
        impl_->queue.push_back({std::move(job), &counter});
    } catch (...) {
        // Not queued (allocation failed): the job will never run, so it must not count, or wait() never returns.
        counter.pending_.fetch_sub(1, std::memory_order_acq_rel);
        throw;
    }
    impl_->cv.notify_all();
}

void JobSystem::parallel_for(JobCounter& counter, std::size_t count, std::size_t batch,
                             RangeJob job) {
    if (count == 0) {
        return;
    }
    batch = std::max<std::size_t>(batch, 1);
    auto shared = std::make_shared<RangeJob>(std::move(job));
    for (std::size_t begin = 0; begin < count; begin += batch) {
        const std::size_t end = std::min(begin + batch, count);
        submit(counter, [shared, begin, end] { (*shared)(begin, end); });
    }
}

void JobSystem::wait(JobCounter& counter) {
    while (!counter.done()) {
        std::optional<QueuedJob> job;
        {
            std::unique_lock lock(impl_->mutex);
            impl_->cv.wait(lock,
                           [&] { return counter.done() || impl_->has_job_for_locked(counter); });
            if (counter.done()) {
                break;
            }
            job = impl_->try_pop_for_locked(counter);
        }
        if (job) {
            impl_->execute(*job);
        }
    }

    std::exception_ptr error;
    {
        std::scoped_lock lock(counter.error_mutex_);
        error = std::exchange(counter.error_, nullptr);
    }
    if (error) {
        std::rethrow_exception(error);
    }
}

unsigned JobSystem::worker_count() const noexcept {
    return static_cast<unsigned>(impl_->workers.size());
}

} // namespace astral
