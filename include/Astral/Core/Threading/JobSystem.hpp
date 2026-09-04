#pragma once

#include <functional>
#include <memory>
#include <atomic>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <algorithm>

namespace Astral {

struct JobCounter {
    std::atomic<uint32_t> count{0};
};

using JobHandle = std::shared_ptr<JobCounter>;

/// Naughty Dog ve modern oyun motoru mimarisine dayali, atomik sayacli,
/// Work-Helping ve non-blocking C++20 Job System.
class JobSystem {
public:
    JobSystem();
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    /// Is parcacigi havuzunu baslatir. workerCount == 0 ise std::max(1u, hardware_concurrency() - 1) kullanilir.
    void Initialize(uint32_t workerCount = 0);

    /// Bekleyen islerin bitmesini bekleyip worker thread'leri guvenle kapatir.
    void Shutdown();

    /// Yeni bir is olusturur ve kuyruga atar. Dondurulen JobHandle ile tamamlanma durumu sorgulanabilir.
    JobHandle Dispatch(std::function<void()> job);

    /// Mevcut bir ebeveyn handle'a bagli alt is olusturur. Ebeveyn sayaci is tamamlandiginda duser.
    void Dispatch(JobHandle parent, std::function<void()> job);

    /// Handle'in isaret ettigi tum isler bitene kadar Work-Helping ile bekler (thread uyumaz, kuyruktan is calistirir).
    void Wait(const JobHandle& handle);

    /// Bir gorevin tamamlanip tamamlanmadigini kontrol eder.
    [[nodiscard]] bool IsDone(const JobHandle& handle) const noexcept;

    /// Kuyruktan tek bir is cekip calistirmayi dener (Work-Helping mekanizmasi).
    /// Is calistirildiysa true, kuyruk bos veya kilit alinamadiysa false doner.
    bool ExecuteOneJob();

    /// Verilen araligi batch'lere bolerek paralel worker'lara dagitir ve tamamlanana kadar bekler.
    template<typename Func>
    void ParallelFor(size_t count, size_t batchSize, Func&& fn) {
        if (count == 0) return;
        if (batchSize == 0) batchSize = 1;

        if (count <= batchSize || m_Workers.empty()) {
            for (size_t i = 0; i < count; ++i) {
                fn(i);
            }
            return;
        }

        auto handle = std::make_shared<JobCounter>();
        size_t batchCount = (count + batchSize - 1) / batchSize;
        handle->count.store(static_cast<uint32_t>(batchCount), std::memory_order_release);

        for (size_t b = 0; b < batchCount; ++b) {
            size_t start = b * batchSize;
            size_t end = std::min(start + batchSize, count);

            PushJobInternal([start, end, &fn, handle]() {
                for (size_t i = start; i < end; ++i) {
                    fn(i);
                }
                handle->count.fetch_sub(1, std::memory_order_acq_rel);
            });
        }

        Wait(handle);
    }

    [[nodiscard]] uint32_t GetWorkerCount() const noexcept { return static_cast<uint32_t>(m_Workers.size()); }
    [[nodiscard]] bool IsInitialized() const noexcept { return m_Running.load(std::memory_order_relaxed); }

    // TaskGraph ve dahili kullanim icin
    void PushJobInternal(std::function<void()> task, JobHandle counter = nullptr);

private:
    struct JobItem {
        std::function<void()> task;
        JobHandle counter;
    };

    void WorkerLoop();

    std::vector<std::thread> m_Workers;
    std::deque<JobItem> m_Queue;
    mutable std::mutex m_QueueMutex;
    std::condition_variable m_Condition;
    std::atomic<bool> m_Running{false};
};

} // namespace Astral
