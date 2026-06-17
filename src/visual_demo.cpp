#include "task_scheduler/task_scheduler.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace {
const char* color(std::size_t w) {
    static const char* c[] = {"\033[31m", "\033[32m", "\033[33m",
                              "\033[34m", "\033[35m", "\033[36m"};
    return c[w % 6];
}
const char* reset = "\033[0m";
}  // namespace

int main() {
    const std::size_t workers = 4;
    ts::TaskScheduler pool(workers);
    std::mutex io;
    std::array<std::atomic<int>, 8> ran{};

    std::cout << "4 workers, 12 tasks of uneven length.\n"
              << "Some workers finish early and STEAL work from busier ones.\n\n";

    auto start = std::chrono::steady_clock::now();
    std::vector<std::future<void>> fs;
    for (int i = 0; i < 12; ++i) {
        int ms = 50 + (i % 4) * 70;  // 50,120,190,260 ms — deliberately uneven
        fs.push_back(pool.submit([&pool, &io, &ran, i, ms] {
            std::size_t w = pool.current_worker();
            ran[w].fetch_add(1, std::memory_order_relaxed);
            {
                std::lock_guard<std::mutex> lk(io);
                std::cout << color(w) << "  worker " << w << reset
                          << "  ran task " << i << "  (" << ms << " ms)\n";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }));
    }
    for (auto& f : fs) f.get();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start).count();

    std::cout << "\nfinished in " << ms << " ms\n";
    std::cout << "tasks per worker:";
    for (std::size_t w = 0; w < workers; ++w)
        std::cout << "  " << color(w) << "w" << w << "=" << ran[w].load()
                  << reset;
    std::cout << "\ntasks taken by stealing: " << pool.steal_count() << "\n";
}