#include "Astral/Core/Threading/JobSystem.hpp"
#include <iostream>

namespace Astral {

JobSystem::JobSystem() = default;

JobSystem::~JobSystem() {
    Shutdown();
}

void JobSystem::Initialize(uint32_t workerCount) {
    if (m_Running.load(std::memory_order_relaxed)) {
        return;
    }

    if (workerCount == 0) {
        uint32_t hw = std::thread::hardware_concurrency();
        workerCount = (hw > 1) ? (hw - 1) : 1;
    }

    m_Running.store(true, std::memory_order_release);
    m_Workers.reserve(workerCount);

    for (uint32_t i = 0; i < workerCount; ++i) {
        m_Workers.emplace_back(&JobSystem::WorkerLoop, this);
    }

    std::cout << "[Astral::JobSystem] Baslatildi (" << workerCount << " worker thread).\n";
}

void JobSystem::Shutdown() {
    if (!m_Running.load(std::memory_order_relaxed)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_Running.store(false, std::memory_order_release);
    }
    m_Condition.notify_all();

    for (auto& worker : m_Workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_Workers.clear();

    // Kalan isler varsa temizle
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_Queue.clear();
    }

    std::cout << "[Astral::JobSystem] Guvenle kapatildi.\n";
}

JobHandle JobSystem::Dispatch(std::function<void()> job) {
    auto handle = std::make_shared<JobCounter>();
    handle->count.store(1, std::memory_order_release);

    PushJobInternal(std::move(job), handle);
    return handle;
}

void JobSystem::Dispatch(JobHandle parent, std::function<void()> job) {
    if (parent) {
        parent->count.fetch_add(1, std::memory_order_acq_rel);
    }
    PushJobInternal(std::move(job), parent);
}

void JobSystem::PushJobInternal(std::function<void()> task, JobHandle counter) {
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_Queue.push_back(JobItem{std::move(task), std::move(counter)});
    }
    m_Condition.notify_one();
}

bool JobSystem::Wait(const JobHandle& handle, std::chrono::milliseconds timeout) {
    if (!handle) return true;

    const bool hasTimeout = (timeout != std::chrono::milliseconds::max());
    const auto startTime = std::chrono::steady_clock::now();

    while (handle->count.load(std::memory_order_acquire) > 0) {
        if (hasTimeout) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime) >= timeout) {
                return false;
            }
            std::this_thread::yield();
        } else {
            // Work-Helping: Bekleyen thread bosta durmaz, kuyruktan is calistirir
            if (!ExecuteOneJob()) {
                std::this_thread::yield();
            }
        }
    }

    return true;
}

bool JobSystem::IsDone(const JobHandle& handle) const noexcept {
    if (!handle) return true;
    return handle->count.load(std::memory_order_acquire) == 0;
}

bool JobSystem::ExecuteOneJob() {
    JobItem item;
    {
        std::unique_lock<std::mutex> lock(m_QueueMutex, std::try_to_lock);
        if (!lock.owns_lock() || m_Queue.empty()) {
            return false;
        }
        item = std::move(m_Queue.front());
        m_Queue.pop_front();
    }

    try {
        if (item.task) {
            item.task();
        }
    } catch (...) {
        if (item.counter) {
            item.counter->count.fetch_sub(1, std::memory_order_acq_rel);
        }
        throw;
    }

    if (item.counter) {
        item.counter->count.fetch_sub(1, std::memory_order_acq_rel);
    }

    return true;
}

void JobSystem::WorkerLoop() {
    while (true) {
        JobItem item;
        {
            std::unique_lock<std::mutex> lock(m_QueueMutex);
            m_Condition.wait(lock, [this]() {
                return !m_Running.load(std::memory_order_relaxed) || !m_Queue.empty();
            });

            if (!m_Running.load(std::memory_order_relaxed) && m_Queue.empty()) {
                break;
            }

            if (!m_Queue.empty()) {
                item = std::move(m_Queue.front());
                m_Queue.pop_front();
            }
        }

        try {
            if (item.task) {
                item.task();
            }
        } catch (const std::exception& e) {
            std::cerr << "[Astral::JobSystem Worker Hata]: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "[Astral::JobSystem Worker Hata]: Bilinmeyen istisna yakalandi!\n";
        }

        if (item.counter) {
            item.counter->count.fetch_sub(1, std::memory_order_acq_rel);
        }
    }
}

} // namespace Astral
