#include "task_scheduler/task_scheduler.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <future>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// Holding task results in a volatile sink stops the optimizer from deleting
// the work as dead code.
volatile std::uint64_t g_sink = 0;

// A CPU-bound unit of work, sized so scheduling overhead doesn't dominate the
// measurement. Returns a value so the loop can't be optimized away.
std::uint64_t spin(std::uint64_t n) {
    std::uint64_t acc = 0;
    for (std::uint64_t i = 0; i < n; ++i) acc += (i * 2654435761u) ^ (acc >> 3);
    return acc;
}

// Submit `count` identical tasks across `workers` threads; return tasks/second.
double throughput(std::size_t workers, int count, std::uint64_t work) {
    ts::TaskScheduler pool(workers);
    std::vector<std::future<std::uint64_t>> fs;
    fs.reserve(count);

    auto start = Clock::now();
    for (int i = 0; i < count; ++i)
        fs.push_back(pool.submit([work] { return spin(work); }));
    std::uint64_t sink = 0;
    for (auto& f : fs) sink += f.get();
    double secs = std::chrono::duration<double>(Clock::now() - start).count();

    g_sink = sink;
    return count / secs;
}

}  // namespace

int main() {
    const int count = 4000;
    const std::uint64_t work = 100000;

    std::printf("workload: %d tasks, %llu work-units each\n\n", count,
                static_cast<unsigned long long>(work));
    std::printf("%-9s %-14s %s\n", "workers", "tasks/sec", "speedup");

    double base = 0;
    for (std::size_t w : {std::size_t(1), std::size_t(2), std::size_t(4),
                          std::size_t(6), std::size_t(8)}) {
        double best = 0;
        for (int rep = 0; rep < 3; ++rep)  // best of 3 to reduce noise
            best = std::max(best, throughput(w, count, work));
        if (w == 1) base = best;
        std::printf("%-9zu %-14.0f %.2fx\n", w, best, best / base);
    }
    return 0;
}