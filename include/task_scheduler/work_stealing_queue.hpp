#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <utility>

namespace ts {

// A task queue owned by a single worker. The owner pushes and pops at the
// front (LIFO, which keeps recently-created work hot in cache); other workers
// steal from the back (FIFO). Each queue carries its own mutex, so two workers
// only contend when they touch the same queue.
class WorkStealingQueue {
public:
    using Task = std::function<void()>;

    void push(Task task) {
        std::lock_guard<std::mutex> lock(mutex_);
        deque_.push_front(std::move(task));
    }

    bool pop(Task& task) {            // owner takes from the front
        std::lock_guard<std::mutex> lock(mutex_);
        if (deque_.empty()) return false;
        task = std::move(deque_.front());
        deque_.pop_front();
        return true;
    }

    bool steal(Task& task) {          // thief takes from the back
        std::lock_guard<std::mutex> lock(mutex_);
        if (deque_.empty()) return false;
        task = std::move(deque_.back());
        deque_.pop_back();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return deque_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::deque<Task> deque_;
};

}  // namespace ts