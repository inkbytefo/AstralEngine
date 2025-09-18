// ThreadPool.cpp
// inkbytefo - AstralEngine
#include "ThreadPool.h"

namespace AstralEngine {

ThreadPool::ThreadPool(size_t num_threads) : m_stop(false) {
    for (size_t i = 0; i < num_threads; ++i) {
        m_workers.emplace_back([this] {
            for (;;) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->m_queueMutex);
                    // Wait until the pool is stopped or there are tasks to do
                    this->m_condition.wait(lock, [this] {
                        return this->m_stop || !this->m_tasks.empty();
                    });

                    // If the pool is stopped and there are no more tasks, exit the thread
                    if (this->m_stop && this->m_tasks.empty()) {
                        return;
                    }

                    // Get the next task from the queue
                    task = std::move(this->m_tasks.front());
                    this->m_tasks.pop();
                }

                // Execute the task
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        // Signal all threads to stop
        m_stop = true;
    }

    // Wake up all threads so they can see the stop signal
    m_condition.notify_all();

    // Wait for all threads to finish their execution
    for (std::thread& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

} // namespace AstralEngine
