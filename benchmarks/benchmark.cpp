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

double median(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    const std::size_t n = samples.size();
    return n % 2 ? samples[n / 2]
                 : (samples[n / 2 - 1] + samples[n / 2]) / 2.0;
}

}  // namespace

int main() {
    const int count = 4000;
    const std::uint64_t work = 100000;
    const int reps = 10;

    std::printf("workload: %d tasks, %llu work-units each\n", count,
                static_cast<unsigned long long>(work));
    // Median rather than best-of-N: the fastest run is the one that happened to
    // catch the coolest thermal state, so taking it overstates what the pool
    // sustains. Min and max are reported so the spread is visible instead of
    // hidden.
    std::printf("sampling: %d runs per worker count, median reported\n\n", reps);
    std::printf("%-9s %-12s %-12s %-12s %s\n", "workers", "median", "min",
                "max", "speedup");

    double base = 0;
    for (std::size_t w : {std::size_t(1), std::size_t(2), std::size_t(4),
                          std::size_t(6), std::size_t(8)}) {
        std::vector<double> samples;
        samples.reserve(reps);
        for (int rep = 0; rep < reps; ++rep)
            samples.push_back(throughput(w, count, work));

        const double med = median(samples);
        const auto [lo, hi] =
            std::minmax_element(samples.begin(), samples.end());
        if (w == 1) base = med;
        std::printf("%-9zu %-12.0f %-12.0f %-12.0f %.2fx\n", w, med, *lo, *hi,
                    med / base);
    }
    return 0;
}