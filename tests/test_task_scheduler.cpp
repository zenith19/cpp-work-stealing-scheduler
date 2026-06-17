#include "task_scheduler/task_scheduler.hpp"

#include <atomic>
#include <future>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                   \
    do {                                                              \
        ++g_checks;                                                   \
        if (!(cond)) {                                                \
            ++g_failures;                                             \
            std::cerr << "  FAILED: " #cond " (line " << __LINE__     \
                      << ")\n";                                       \
        }                                                             \
    } while (0)

void test_returns_value() {
    ts::TaskScheduler pool(2);
    auto f = pool.submit([] { return 7 * 6; });
    CHECK(f.get() == 42);
}

void test_runs_every_task() {
    ts::TaskScheduler pool(4);
    const int n = 10000;
    std::vector<std::future<int>> fs;
    for (int i = 0; i < n; ++i) fs.push_back(pool.submit([i] { return i; }));
    long long sum = 0;
    for (auto& f : fs) sum += f.get();
    CHECK(sum == static_cast<long long>(n - 1) * n / 2);
}

void test_exception_propagates() {
    ts::TaskScheduler pool(2);
    auto f = pool.submit([]() -> int { throw std::runtime_error("boom"); });
    bool threw = false;
    try {
        f.get();
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}

void test_shutdown_drains_pending() {
    std::atomic<int> ran{0};
    {
        ts::TaskScheduler pool(4);
        for (int i = 0; i < 1000; ++i)
            pool.submit([&ran] { ran.fetch_add(1, std::memory_order_relaxed); });
    }  // destructor must finish all queued work
    CHECK(ran.load() == 1000);
}

void test_zero_threads_is_safe() {
    ts::TaskScheduler pool(0);
    CHECK(pool.size() >= 1);
    auto f = pool.submit([] { return 1; });
    CHECK(f.get() == 1);
}

void test_void_tasks() {
    ts::TaskScheduler pool(3);
    std::atomic<int> c{0};
    std::vector<std::future<void>> fs;
    for (int i = 0; i < 100; ++i)
        fs.push_back(pool.submit([&c] { c.fetch_add(1, std::memory_order_relaxed); }));
    for (auto& f : fs) f.get();
    CHECK(c.load() == 100);
}

}  // namespace

int main() {
    test_returns_value();
    test_runs_every_task();
    test_exception_propagates();
    test_shutdown_drains_pending();
    test_zero_threads_is_safe();
    test_void_tasks();

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    if (g_failures > 0) {
        std::cout << g_failures << " FAILURE(S)\n";
        return 1;
    }
    std::cout << "ALL TESTS PASSED\n";
    return 0;
}