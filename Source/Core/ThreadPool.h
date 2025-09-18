// ThreadPool.h
// inkbytefo - AstralEngine
#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>

namespace AstralEngine {

/**
 * @brief A simple, general-purpose, and thread-safe thread pool.
 * 
 * This class manages a pool of worker threads that can execute tasks
 * submitted to a queue. It is designed for asynchronous operations,
 * such as asset loading, to prevent blocking the main thread.
 */
class ThreadPool {
public:
    /**
     * @brief Constructs a ThreadPool with a specified number of worker threads.
     * @param num_threads The number of worker threads to create.
     */
    explicit ThreadPool(size_t num_threads);

    /**
     * @brief Destructor. Stops all worker threads and waits for them to finish.
     */
    ~ThreadPool();

    // Non-copyable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief Submits a task to the thread pool for execution.
     * 
     * @tparam F The type of the callable object (function, lambda, etc.).
     * @tparam Args The types of the arguments to pass to the callable.
     * @param f The callable object to execute.
     * @param args The arguments to pass to the callable.
     * @return A std::future that will hold the result of the task.
     */
    template<class F, class... Args>
    auto Submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;

private:
    // Worker threads
    std::vector<std::thread> m_workers;
    // Task queue
    std::queue<std::function<void()>> m_tasks;
    // Mutex for synchronizing access to the task queue
    std::mutex m_queueMutex;
    // Condition variable for signaling worker threads
    std::condition_variable m_condition;
    // Flag to stop the thread pool
    std::atomic<bool> m_stop;
};

// Template implementation for Submit
template<class F, class... Args>
auto ThreadPool::Submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
    using ReturnType = decltype(f(args...));

    // Create a packaged_task to wrap the function and its arguments
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<ReturnType> result = task->get_future();
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);

        // Don't allow enqueueing after stopping the pool
        if (m_stop) {
            throw std::runtime_error("Submit on stopped ThreadPool");
        }

        // Wrap the task in a void() function to be stored in the queue
        m_tasks.emplace([task]() {
            (*task)();
        });
    }

    // Signal one worker thread that a new task is available
    m_condition.notify_one();
    return result;
}

} // namespace AstralEngine
