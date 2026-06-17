#include "task_scheduler/task_scheduler.hpp"

#include <chrono>

namespace ts {
namespace {
constexpr auto kIdleWait = std::chrono::milliseconds(10);
}

TaskScheduler::TaskScheduler(std::size_t num_threads) {
    if (num_threads == 0) num_threads = 1;

    queues_.reserve(num_threads);
    for (std::size_t i = 0; i < num_threads; ++i)
        queues_.push_back(std::make_unique<WorkStealingQueue>());

    workers_.reserve(num_threads);
    for (std::size_t i = 0; i < num_threads; ++i)
        workers_.emplace_back(
            [this, i](std::stop_token stoken) { worker_loop(stoken, i); });
}

TaskScheduler::~TaskScheduler() {
    for (auto& worker : workers_) worker.request_stop();
    wait_cv_.notify_all();
}

void TaskScheduler::enqueue(std::function<void()> task) {
    if (this_pool_ == this) {
        queues_[this_index_]->push(std::move(task));  // keep our own work local
    } else {
        std::size_t i =
            next_queue_.fetch_add(1, std::memory_order_relaxed) % queues_.size();
        queues_[i]->push(std::move(task));
    }
    wait_cv_.notify_one();
}

bool TaskScheduler::try_get_task(std::size_t index,
                                 std::function<void()>& task) {
    if (queues_[index]->pop(task)) return true;        // our own queue first

    std::size_t n = queues_.size();
    for (std::size_t offset = 1; offset < n; ++offset) {
        std::size_t victim = (index + offset) % n;
        if (queues_[victim]->steal(task)) {
            steals_.fetch_add(1, std::memory_order_relaxed);
            return true;
        } // then steal
    }
    return false;
}

void TaskScheduler::worker_loop(std::stop_token stoken, std::size_t index) {
    this_pool_ = this;
    this_index_ = index;

    while (true) {
        std::function<void()> task;
        if (try_get_task(index, task)) {
            task();
            continue;
        }
        if (stoken.stop_requested()) return;  // nothing left and shutting down

        // No work anywhere. Sleep until notified, with a short timeout as a
        // backstop: queue locks are decoupled from wait_mutex_, so a wakeup
        // can race in just after we check, and the timeout keeps us from
        // sleeping through it.
        std::unique_lock<std::mutex> lock(wait_mutex_);
        wait_cv_.wait_for(lock, stoken, kIdleWait, [this] {
            for (auto& q : queues_)
                if (!q->empty()) return true;
            return false;
        });
    }
}

}  // namespace ts