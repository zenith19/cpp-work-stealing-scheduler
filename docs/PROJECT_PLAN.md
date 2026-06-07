# Multithreaded Task Scheduler + Benchmarking

## Project Overview

A high-performance task execution engine that efficiently distributes work across CPU cores. Built from scratch in modern C++ to demonstrate systems programming proficiency, concurrency mastery, and performance-oriented thinking.

---

## Goals

1. **Learn & demonstrate** advanced C++ concurrency patterns
2. **Build production-quality** code with tests and documentation
3. **Prove performance** with benchmarks and comparisons
4. **Create portfolio piece** that signals big-tech readiness

---

## Core Features

| Feature | Description | Priority |
|---------|-------------|----------|
| Thread Pool | Fixed pool of reusable worker threads | Must have |
| Task Queue | Thread-safe queue for pending tasks | Must have |
| Futures/Promises | Return values from async tasks | Must have |
| Graceful Shutdown | Clean termination with pending task handling | Must have |
| Work-Stealing | Workers steal tasks from other workers' queues | Must have |
| Benchmarking | Performance measurement and comparison | Must have |

---

## Stretch Features (Optional)

| Feature | Description | Impact |
|---------|-------------|--------|
| Task Priorities | High-priority tasks execute first | Medium |
| Adaptive Sizing | Pool grows/shrinks based on load | High |
| Task Dependencies | DAG-based execution (task B waits for A) | High |
| Metrics Endpoint | Real-time stats via HTTP | Medium |

---

## Technical Stack

| Component | Choice | Rationale |
|-----------|--------|-----------|
| Language | C++20 | Modern features: `std::jthread`, `std::stop_token`, concepts, `std::atomic::wait()` |
| Build System | CMake | Industry standard, cross-platform |
| Testing | Google Test | Widely used, good documentation |
| Benchmarking | Google Benchmark | Industry standard for C++ microbenchmarks |
| Sanitizers | ThreadSanitizer, AddressSanitizer | Catch race conditions and memory bugs |
| CI/CD | GitHub Actions | Free, integrates with repo |

---

## C++20 Features We'll Use

| Feature | Where Used | Benefit |
|---------|------------|---------|
| `std::jthread` | Worker threads | Auto-joining, cleaner lifecycle management |
| `std::stop_token` | Shutdown | Cooperative cancellation without flags |
| `std::stop_source` | TaskScheduler | Signal workers to stop cleanly |
| `std::atomic::wait()` / `notify_one()` | Work-stealing deque | More efficient than condition variables |
| Concepts | `submit()` template | Clean constraints on callable types |
| `std::latch` | Testing | Synchronize test threads |
| `std::span` | Internal APIs | Safer array/buffer passing |
| Designated initializers | Config structs | Cleaner initialization |

---

## Architecture Overview

![Architecture Diagram](./docs/architecture_diagram.png)

### Key Components

| Component | Responsibility |
|-----------|----------------|
| `TaskScheduler` | Main class, public API, owns workers |
| `Worker` | Thread wrapper, executes tasks, steals when idle |
| `WorkStealingDeque` | Lock-free (or fine-grained) double-ended queue |
| `Task` | Type-erased callable wrapper |

---

## Implementation Phases

### Phase 1: Project Setup (Day 1)

**Objective:** Build infrastructure that compiles and runs

- [ ] Initialize Git repository
- [ ] Create folder structure (see below)
- [ ] Set up CMake with targets: library, tests, benchmarks
- [ ] Add GitHub Actions CI for build verification
- [ ] Verify "Hello World" compiles and runs

**Folder Structure:**

```
task-scheduler/
│
├── CMakeLists.txt
├── README.md
├── PROJECT_PLAN.md
│
├── docs/
│   └── architecture_diagram.png
│
├── include/
│   └── task_scheduler/
│       ├── task_scheduler.hpp
│       ├── worker.hpp
│       └── work_stealing_deque.hpp
│
├── src/
│   ├── task_scheduler.cpp
│   └── worker.cpp
│
├── tests/
│   └── test_task_scheduler.cpp
│
├── benchmarks/
│   └── benchmark_scheduler.cpp
│
└── .github/
    └── workflows/
        └── ci.yml
```

**Exit Criteria:** `cmake --build . && ./hello` works

---

### Phase 2: Basic Thread Pool (Days 2–5)

**Objective:** Pool that executes tasks across threads

- [ ] Implement `TaskScheduler` class with fixed thread count
- [ ] Use `std::jthread` for auto-joining worker threads
- [ ] Create shared task queue with `std::queue` + `std::mutex`
- [ ] Implement `submit()` accepting lambdas/callables
- [ ] Use `std::stop_token` for cooperative shutdown
- [ ] Workers wait on `std::condition_variable` when idle

**API at this stage:**
```cpp
TaskScheduler pool(4);
pool.submit([]{ std::cout << "Task executed\n"; });
// Destructor handles shutdown automatically via std::jthread
```

**Exit Criteria:** Tasks submitted from main thread execute on worker threads

---

### Phase 3: Futures & Return Values (Days 6–8)

**Objective:** Callers can retrieve task results

- [ ] `submit()` returns `std::future<T>`
- [ ] Wrap callables in `std::packaged_task`
- [ ] Use concepts to constrain callable types (`std::invocable`)
- [ ] Support different return types via templates
- [ ] Implement perfect forwarding for task arguments

**API at this stage:**
```cpp
TaskScheduler pool(4);
auto future = pool.submit([]{ return 42; });
int result = future.get();  // 42
```

**Exit Criteria:** Can submit tasks with return values and retrieve them

---

### Phase 4: Work-Stealing (Days 9–13)

**Objective:** Load-balanced scheduling via work-stealing

- [ ] Replace shared queue with per-worker local deques
- [ ] Implement `WorkStealingDeque` using `std::atomic::wait()` / `notify_one()`
- [ ] Workers pop from own deque (LIFO for cache locality)
- [ ] Idle workers steal from others' deques (FIFO)
- [ ] Handle contention during stealing with backoff

**Algorithm:**
```
while not stop_requested:
    task = local_deque.pop_bottom()      # Try own queue first
    if task is null:
        task = steal_from_random_victim() # Steal if empty
    if task is not null:
        execute(task)
    else:
        wait_briefly()                    # Back off if nothing to do
```

**Exit Criteria:** Uneven workloads distribute across workers efficiently

---

### Phase 5: Graceful Shutdown & Edge Cases (Days 14–15)

**Objective:** Robust handling of real-world scenarios

- [ ] `shutdown()` — stop accepting tasks, complete pending ones
- [ ] `shutdown_now()` — stop immediately, discard pending tasks
- [ ] Reject submissions after shutdown (throw or return invalid future)
- [ ] Exception propagation: task throws → future.get() rethrows
- [ ] Handle edge cases:
  - Submit after shutdown
  - Zero-thread pool
  - Task submits more tasks

**Exit Criteria:** Pool handles all edge cases without crashes or hangs

---

### Phase 6: Testing (Days 16–19)

**Objective:** Prove correctness with comprehensive tests

#### Unit Tests
- [ ] Submit single task, verify execution
- [ ] Submit multiple tasks, verify all execute
- [ ] Verify return values via futures
- [ ] Verify exception propagation
- [ ] Shutdown while tasks pending
- [ ] Shutdown with no pending tasks

#### Stress Tests
- [ ] Submit 100,000+ tasks, verify all complete
- [ ] Multiple threads submitting concurrently (use `std::latch` for sync)
- [ ] Tasks that submit more tasks (recursive)

#### Sanitizer Tests
- [ ] Run full suite with ThreadSanitizer
- [ ] Run full suite with AddressSanitizer
- [ ] Fix any reported races or memory issues

**Exit Criteria:** All tests pass, zero sanitizer warnings

---

### Phase 7: Benchmarking (Days 20–23)

**Objective:** Measure and prove performance

#### Benchmarks to Implement

| Benchmark | Description |
|-----------|-------------|
| Throughput | Tasks completed per second |
| Latency | Time from submit to completion (avg, p50, p95, p99) |
| Scaling | Throughput vs thread count (1, 2, 4, 8, 16) |
| Work-stealing benefit | Even vs uneven workload comparison |

#### Comparisons

| Baseline | Why Compare |
|----------|-------------|
| Single-threaded | Show parallelism benefit |
| `std::async` | Common naive approach |
| No work-stealing (your own pool) | Show work-stealing benefit |

#### Deliverables
- [ ] Benchmark executable with Google Benchmark
- [ ] Script to generate performance data
- [ ] Charts/graphs for README (throughput, scaling curves)

**Exit Criteria:** Clear data showing performance characteristics

---

### Phase 8: Documentation & Polish (Days 24–25)

**Objective:** Portfolio-ready repository

#### README Contents
- [ ] Project title and one-line description
- [ ] Badges (build status, license)
- [ ] Features list
- [ ] Quick start / usage example
- [ ] Build instructions
- [ ] Benchmark results with graphs
- [ ] Design decisions and tradeoffs
- [ ] Future improvements / roadmap
- [ ] License

#### Code Polish
- [ ] Consistent formatting (clang-format)
- [ ] Comments on non-obvious code
- [ ] Remove dead code and TODOs
- [ ] Clean Git history (squash WIP commits)

**Exit Criteria:** Someone can clone, build, understand, and be impressed

---

## Success Criteria

| Criteria | Target |
|----------|--------|
| Correctness | All tests pass, zero sanitizer warnings |
| Performance | Outperforms `std::async` by 2x+ on high-volume tasks |
| Scaling | Near-linear scaling up to physical core count |
| Code Quality | Modern C++20, no raw pointers, RAII everywhere |
| Documentation | README explains what, why, and how |

---

## Timeline Summary

| Phase | Duration | Cumulative |
|-------|----------|------------|
| 1. Project Setup | 1 day | Day 1 |
| 2. Basic Thread Pool | 4 days | Day 5 |
| 3. Futures & Return Values | 3 days | Day 8 |
| 4. Work-Stealing | 5 days | Day 13 |
| 5. Graceful Shutdown | 2 days | Day 15 |
| 6. Testing | 4 days | Day 19 |
| 7. Benchmarking | 4 days | Day 23 |
| 8. Documentation | 2 days | Day 25 |

**Total: ~25 days / 3.5 weeks**

---

## References & Resources

### Learning Resources
- [C++ Concurrency in Action (Book)](https://www.manning.com/books/c-plus-plus-concurrency-in-action) — Williams
- [Intel TBB Work-Stealing](https://software.intel.com/content/www/us/en/develop/documentation/tbb-documentation/top.html)
- [Chase-Lev Deque Paper](https://www.dre.vanderbilt.edu/~schmidt/PDF/work-stealing-dequeue.pdf)

### Reference Implementations
- [Intel TBB](https://github.com/oneapi-src/oneTBB)
- [Folly Executors](https://github.com/facebook/folly/tree/main/folly/executors)
- [Boost.Asio](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html)

---

## Notes

- Start simple, iterate
- Commit often with clear messages
- Don't optimize prematurely—make it work, then make it fast
- Ask for feedback on API design early
