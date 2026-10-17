#include "task_scheduler/task_scheduler.hpp"

#include <cstdint>
#include <stdexcept>

namespace ts {
namespace {

// Per-worker PRNG for picking a steal victim. Seeded in worker_loop from the
// worker index, so no two workers walk the ring in the same order.
thread_local std::uint64_t rng_state = 0;

std::uint64_t next_random() {
    std::uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return x;
}

}  // namespace

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

TaskScheduler::~TaskScheduler() { stop_and_join(); }

void TaskScheduler::shutdown() {
    if (this_pool_ == this)
        throw std::runtime_error(
            "TaskScheduler::shutdown() called from a worker thread");
    stop_and_join();
}

void TaskScheduler::stop_and_join() noexcept {
    std::call_once(shutdown_once_, [this] {
        stopping_.store(true, std::memory_order_release);
        for (auto& worker : workers_) worker.request_stop();
        wait_cv_.notify_all();
        // Workers exit only once every queue is empty, so this drains the pool.
        for (auto& worker : workers_) worker.join();
    });
}

void TaskScheduler::enqueue(std::function<void()> task) {
    const bool is_own_worker = (this_pool_ == this);

    // Our own workers may keep submitting while we drain; anyone else would be
    // handing us work no one is left to run.
    if (!is_own_worker && stopping_.load(std::memory_order_acquire))
        throw std::runtime_error("TaskScheduler is shutting down");

    // Published before the push so an idle worker never sees an empty counter
    // alongside a non-empty queue. The reverse skew is harmless: a worker that
    // wakes early finds nothing and loops.
    pending_.fetch_add(1, std::memory_order_seq_cst);

    // The count has to come back down if the push throws. A pending_ that
    // never reaches zero leaves every idle worker spinning on a predicate
    // that is permanently true, which is far worse than the failed submit.
    try {
        if (is_own_worker) {
            queues_[this_index_]->push(std::move(task));  // keep our work local
        } else {
            std::size_t i = next_queue_.fetch_add(1, std::memory_order_relaxed) %
                            queues_.size();
            queues_[i]->push(std::move(task));
        }
    } catch (...) {
        pending_.fetch_sub(1, std::memory_order_seq_cst);
        throw;
    }

    // This load and the sleepers_ increment in worker_loop are a Dekker pair,
    // which is why both are seq_cst: either we see a sleeper here, or that
    // worker sees our pending_ increment and doesn't sleep. Taking wait_mutex_
    // before notifying closes the remaining window, where a worker has
    // announced itself but has not reached wait() yet. While the pool is busy
    // there are no sleepers and none of this is touched.
    if (sleepers_.load(std::memory_order_seq_cst) > 0) {
        { std::lock_guard<std::mutex> guard(wait_mutex_); }
        wait_cv_.notify_one();
    }
}

bool TaskScheduler::try_get_task(std::size_t index,
                                 std::function<void()>& task) {
    if (queues_[index]->pop(task)) {                  // our own queue first
        pending_.fetch_sub(1, std::memory_order_seq_cst);
        return true;
    }

    const std::size_t n = queues_.size();
    if (n > 1) {
        // Random starting point, then the whole ring: every victim is still
        // tried, but idle workers don't all converge on the same neighbour.
        const std::size_t start = next_random() % (n - 1);
        for (std::size_t k = 0; k < n - 1; ++k) {
            const std::size_t offset = 1 + (start + k) % (n - 1);
            const std::size_t victim = (index + offset) % n;
            if (queues_[victim]->try_steal(task)) {   // then steal
                pending_.fetch_sub(1, std::memory_order_seq_cst);
                steals_.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        }
    }
    return false;
}

void TaskScheduler::worker_loop(std::stop_token stoken, std::size_t index) {
    this_pool_ = this;
    this_index_ = index;
    rng_state = 0x9E3779B97F4A7C15ull * (index + 1) + 1;  // never zero

    while (true) {
        std::function<void()> task;
        if (try_get_task(index, task)) {
            task();
            continue;
        }
        if (stoken.stop_requested()) return;  // nothing left and shutting down

        // No work found. Announce ourselves before re-checking: wait() tests
        // the predicate under wait_mutex_ before blocking, so a task enqueued
        // in the meantime is seen here rather than slept through.
        std::unique_lock<std::mutex> lock(wait_mutex_);
        sleepers_.fetch_add(1, std::memory_order_seq_cst);
        wait_cv_.wait(lock, stoken, [this] {
            return pending_.load(std::memory_order_seq_cst) > 0;
        });
        sleepers_.fetch_sub(1, std::memory_order_seq_cst);
    }
}

}  // namespace ts
