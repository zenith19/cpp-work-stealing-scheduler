#include "task_scheduler/task_scheduler.hpp"

#include <future>
#include <iostream>
#include <vector>

int main() {
    ts::TaskScheduler pool(4);
    std::cout << "pool created with " << pool.size() << " workers\n";

    std::vector<std::future<int>> results;
    for (int i = 0; i < 10; ++i) {
        results.push_back(pool.submit([i] { return i * i; }));
    }

    for (int i = 0; i < 10; ++i) {
        std::cout << i << "^2 = " << results[i].get() << "\n";
    }
}