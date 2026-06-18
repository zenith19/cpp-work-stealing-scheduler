# Benchmark results

Hardware: Intel Core i7-1165G7 (4 physical cores / 8 threads), Linux, Release build (-O2).
Workload: 4000 CPU-bound tasks, 100000 work-units each.

| workers | tasks/sec | speedup |
|--------:|----------:|--------:|
| 1       | 4067      | 1.00x   |
| 2       | 8299      | 2.04x   |
| 4       | 16548     | 4.07x   |
| 6       | 22695     | 5.58x   |
| 8       | 25101     | 6.17x   |

Near-linear scaling to 4 workers (matching the 4 physical cores), with
continued gains through 8 threads via hyper-threading.