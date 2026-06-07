//
// Created by zenith on 2026-06-07.
//
#include "task_scheduler/task_scheduler.hpp"

#include <utility>

namespace ts {

    TaskScheduler::TaskScheduler(std::size_t num_threads) {
        // Guard against a zero-thread pool, which could never run anything.
        if (num_threads == 0) {
            num_threads = 1;
        }

        workers_.reserve(num_threads);
        for (std::size_t i = 0; i < num_threads; ++i) {
            // Each jthread runs worker_loop. The jthread passes its own
            // stop_token as the first argument automatically.
            workers_.emplace_back([this](std::stop_token stoken) {
                worker_loop(stoken);
            });
        }
    }

    TaskScheduler::~TaskScheduler() {
        // Ask every worker to stop. request_stop() does two things:
        //   1. flips the stop_token so the loop condition becomes false
        //   2. wakes any worker blocked in cv_.wait(...)
        for (auto& worker : workers_) {
            worker.request_stop();
        }
        cv_.notify_all();
        // jthread destructors join automatically as workers_ is destroyed.
    }

    void TaskScheduler::submit(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push(std::move(task));
        }
        // Wake one sleeping worker to handle the new task.
        cv_.notify_one();
    }

    void TaskScheduler::worker_loop(std::stop_token stoken) {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);

                // Sleep until there is work OR a stop is requested.
                cv_.wait(lock, stoken, [this] { return !tasks_.empty(); });

                // Woken for shutdown with no work left -> exit the loop.
                if (tasks_.empty()) {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }  // lock released here, BEFORE running the task

            task();  // run the task without holding the lock
        }
    }

}  // namespace ts