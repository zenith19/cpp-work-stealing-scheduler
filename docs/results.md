# Benchmark results

Hardware: Intel Core i7-1165G7 (4 physical cores / 8 threads), Linux, GCC 13.3, CMake Release build.
Workload: 4000 CPU-bound tasks, 100000 work-units each. Best of 3 runs per worker count.
Speedup is relative to the same scheduler running with 1 worker.

| workers | tasks/sec | speedup |
|--------:|----------:|--------:|
| 1       | 4067      | 1.00x   |
| 2       | 8299      | 2.04x   |
| 4       | 16548     | 4.07x   |
| 6       | 22695     | 5.58x   |
| 8       | 25101     | 6.17x   |

Near-linear scaling to 4 workers (matching the 4 physical cores), with
continued, smaller gains through 8 hardware threads (hyper-threading).
