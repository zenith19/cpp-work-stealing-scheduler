#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <vector>

namespace ts {

// A fixed-size pool of worker threads that execute submitted tasks.
// Phase 2: one shared queue, no work-stealing and no return values yet.
class TaskScheduler {
public:
    // Create a pool with `num_threads` workers.
    // Defaults to the number of hardware threads the machine reports.
    explicit TaskScheduler(
        std::size_t num_threads = std::thread::hardware_concurrency());

    // Stops all workers and joins them before the object is destroyed.
    ~TaskScheduler();

    // A scheduler owns OS threads, so copying or moving it has no meaning.
    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;
    TaskScheduler(TaskScheduler&&) = delete;
    TaskScheduler& operator=(TaskScheduler&&) = delete;

    // Hand a task to the pool; it runs on some worker thread.
    void submit(std::function<void()> task);

    // Number of worker threads in the pool.
    std::size_t size() const noexcept { return workers_.size(); }

private:
    // The loop every worker thread runs until stop is requested.
    void worker_loop(std::stop_token stoken);

    std::queue<std::function<void()>> tasks_;   // pending tasks
    mutable std::mutex mutex_;                   // protects tasks_
    std::condition_variable_any cv_;             // wakes idle workers
    std::vector<std::jthread> workers_;          // declared LAST on purpose
};

}  // namespace ts
