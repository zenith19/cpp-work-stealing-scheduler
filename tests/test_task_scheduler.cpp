#include "task_scheduler/task_scheduler.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
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

// Work submitted from inside a task lands on that worker's own queue, so the
// only way the other workers get any of it is by stealing. Tasks are long
// enough that one worker cannot plausibly finish them all alone.
void test_stealing_happens() {
    ts::TaskScheduler pool(4);
    std::vector<std::future<void>> inner;

    pool.submit([&pool, &inner] {
        for (int i = 0; i < 12; ++i)
            inner.push_back(pool.submit([] {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }));
    }).get();

    for (auto& f : inner) f.get();
    CHECK(pool.steal_count() > 0);
}

// Several external threads submitting at once. Each task owns one slot and
// writes only to it, so the check is per task rather than a running total: a
// total would still come out right if one task ran twice and another never
// ran at all.
void test_concurrent_submitters() {
    ts::TaskScheduler pool(4);
    const int producers = 8, per_producer = 1000;
    const int total = producers * per_producer;
    std::vector<std::atomic<int>> ran(total);

    std::vector<std::jthread> submitters;
    for (int p = 0; p < producers; ++p)
        submitters.emplace_back([&pool, &ran, p] {
            std::vector<std::future<void>> fs;
            fs.reserve(per_producer);
            for (int i = 0; i < per_producer; ++i) {
                const int id = p * per_producer + i;
                fs.push_back(pool.submit(
                    [&ran, id] { ran[id].fetch_add(1, std::memory_order_relaxed); }));
            }
            for (auto& f : fs) f.get();
        });
    submitters.clear();  // join

    int missing = 0, repeated = 0;
    for (auto& slot : ran) {
        const int n = slot.load(std::memory_order_relaxed);
        if (n == 0) ++missing;
        if (n > 1) ++repeated;
    }
    CHECK(missing == 0);
    CHECK(repeated == 0);
}

void test_recursive_submit() {
    ts::TaskScheduler pool(3);
    auto outer = pool.submit([&pool] {
        auto child = pool.submit([] { return 21; });
        return child.get() * 2;
    });
    CHECK(outer.get() == 42);
}

// Tasks that spawn more work while the pool is winding down. A worker is
// allowed to submit during the drain — only outside submitters are rejected —
// and no worker exits until every queue is empty, so the children have to run
// even though they were created after shutdown began. Nothing waits on the
// parents here: the point is to have work still in flight when the destructor
// starts.
void test_nested_submit_during_shutdown() {
    const int parents = 50;
    std::atomic<int> ran{0};
    {
        ts::TaskScheduler pool(4);
        for (int i = 0; i < parents; ++i)
            pool.submit([&pool, &ran] {
                ran.fetch_add(1, std::memory_order_relaxed);
                for (int c = 0; c < 2; ++c)
                    pool.submit([&ran] {
                        ran.fetch_add(1, std::memory_order_relaxed);
                    });
            });
    }  // drains parents and whatever they spawned on the way out
    CHECK(ran.load() == parents * 3);
}

void test_current_worker_reporting() {
    ts::TaskScheduler pool(3);
    CHECK(pool.current_worker() == ts::TaskScheduler::no_worker);
    auto f = pool.submit([&pool] { return pool.current_worker(); });
    CHECK(f.get() < pool.size());
}

void test_submit_after_shutdown_throws() {
    ts::TaskScheduler pool(2);
    std::atomic<int> ran{0};
    for (int i = 0; i < 100; ++i)
        pool.submit([&ran] { ran.fetch_add(1, std::memory_order_relaxed); });

    pool.shutdown();
    CHECK(ran.load() == 100);  // shutdown drains before returning

    bool threw = false;
    try {
        pool.submit([] {});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);

    // Idempotent, and safe from several threads at once.
    std::vector<std::jthread> closers;
    for (int i = 0; i < 4; ++i)
        closers.emplace_back([&pool] { pool.shutdown(); });
    closers.clear();
}

}  // namespace

int main() {
    test_returns_value();
    test_runs_every_task();
    test_exception_propagates();
    test_shutdown_drains_pending();
    test_zero_threads_is_safe();
    test_void_tasks();
    test_stealing_happens();
    test_concurrent_submitters();
    test_recursive_submit();
    test_nested_submit_during_shutdown();
    test_current_worker_reporting();
    test_submit_after_shutdown_throws();

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    if (g_failures > 0) {
        std::cout << g_failures << " FAILURE(S)\n";
        return 1;
    }
    std::cout << "ALL TESTS PASSED\n";
    return 0;
}