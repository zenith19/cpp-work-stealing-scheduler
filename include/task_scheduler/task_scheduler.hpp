#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "task_scheduler/work_stealing_queue.hpp"

namespace ts {

class TaskScheduler {
public:
    static constexpr std::size_t no_worker = static_cast<std::size_t>(-1);

    explicit TaskScheduler(
        std::size_t num_threads = std::thread::hardware_concurrency());
    ~TaskScheduler();

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;
    TaskScheduler(TaskScheduler&&) = delete;
    TaskScheduler& operator=(TaskScheduler&&) = delete;

    template <typename F>
    auto submit(F&& f) -> std::future<std::invoke_result_t<F>> {
        using Result = std::invoke_result_t<F>;
        auto task = std::make_shared<std::packaged_task<Result()>>(
            std::forward<F>(f));
        std::future<Result> result = task->get_future();
        enqueue([task] { (*task)(); });
        return result;
    }

    std::size_t size() const noexcept { return queues_.size(); }

    // Index of the worker executing the calling code, or no_worker if the
    // caller is not one of this pool's worker threads.
    std::size_t current_worker() const noexcept {
        return (this_pool_ == this) ? this_index_ : no_worker;
    }

    // Number of tasks acquired by stealing rather than from a worker's own
    // queue. Useful for tests, demos, and tuning.
    std::size_t steal_count() const noexcept {
        return steals_.load(std::memory_order_relaxed);
    }

private:
    void enqueue(std::function<void()> task);
    void worker_loop(std::stop_token stoken, std::size_t index);
    bool try_get_task(std::size_t index, std::function<void()>& task);

    std::vector<std::unique_ptr<WorkStealingQueue>> queues_;
    std::mutex wait_mutex_;
    std::condition_variable_any wait_cv_;
    std::atomic<std::size_t> next_queue_{0};
    std::atomic<std::size_t> steals_{0};
    std::vector<std::jthread> workers_;  // declared last: joined first

    inline static thread_local TaskScheduler* this_pool_ = nullptr;
    inline static thread_local std::size_t this_index_ = 0;
};

}  // namespace ts