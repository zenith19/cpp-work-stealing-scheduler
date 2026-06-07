//
// Created by zenith on 2026-06-07.
//
#include "task_scheduler/task_scheduler.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    ts::TaskScheduler pool(4);
    std::cout << "pool created with " << pool.size() << " workers\n";

    std::atomic<int> counter{0};

    // Submit 20 tasks; each bumps the counter.
    for (int i = 0; i < 20; ++i) {
        pool.submit([&counter, i] {
            counter.fetch_add(1, std::memory_order_relaxed);
            std::cout << "task " << i << " ran\n";
        });
    }

    // Give the workers a moment to drain the queue.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "counter = " << counter.load() << " (expected 20)\n";
    // pool's destructor stops and joins the workers here.
}