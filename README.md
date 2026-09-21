Absolutely. Here is the README updated to reflect **today's actual work and findings**, without overstating what the allocator achieved.

# Limit Order Book & Matching Engine

A C++17 implementation of a single-threaded limit order book with price-time priority matching, order cancellation, modification, trade history, automated tests, repeatable benchmarking, profiling, and measurement-driven performance optimization.

## Overview

This project models the core order-matching logic of an electronic trading system. It maintains buy and sell orders, matches incoming orders against the best available prices, and preserves FIFO priority among orders at the same price level.

The project is being developed incrementally, with correctness and understanding of the underlying data structures established before introducing more advanced performance, concurrency, and networking techniques.

The current focus is on building a defensible single-threaded matching engine and using profiling and benchmarking to guide optimization decisions rather than assuming a particular optimization will improve performance.

## Features

* **Price-time priority:** Best price first, then FIFO within each price level.
* **Order matching:** Supports full and partial fills.
* **Order cancellation:** Removes active orders by ID.
* **Order modification:** Supports quantity changes and price changes.
* **Order lookup:** Uses an ID index to locate active orders without scanning the book.
* **Trade history:** Records executed trades with buyer ID, seller ID, execution price, and quantity.
* **Automated tests:** Covers matching and order-management behavior.
* **Benchmarking:** Uses a fixed-seed synthetic workload for repeatable measurements.
* **Profiling:** Uses macOS Instruments / Time Profiler to identify actual runtime hotspots.
* **Memory optimization experiment:** Uses a recycling allocator for `std::list` nodes to reduce repeated heap allocation and deallocation.

## Architecture

### Data structures

| Component                                | Purpose                                    |
| ---------------------------------------- | ------------------------------------------ |
| `std::map` for bids                      | Maintains price levels in descending order |
| `std::map` for asks                      | Maintains price levels in ascending order  |
| `std::list<Order, PoolAllocator<Order>>` | Stores FIFO orders within each price level |
| `std::unordered_map`                     | Maps active order IDs to their locations   |
| `std::vector<Trade>`                     | Stores executed trades                     |

The order ID index stores the order's side, price, and list iterator. This allows an active order to be located without scanning every price level.

`std::list` was chosen over `std::queue` because cancellation may target an order anywhere within a price level. Given a valid iterator, removing an element from the list is O(1).

The list's node-based storage has a memory-allocation tradeoff: nodes are individually allocated rather than stored contiguously. Profiling the original implementation showed that this allocation/deallocation behavior was a measurable part of the total runtime.

### Matching rules

1. A BUY order checks the lowest ask.
2. A SELL order checks the highest bid.
3. Matching continues while the prices cross.
4. The executed quantity is the smaller of the incoming and resting quantities.
5. The resting order's price is used as the execution price.
6. Fully filled resting orders are removed from the book and active-order index.
7. Any remaining incoming quantity is added to the book.

## Complexity

Let `P` be the number of price levels and `K` the number of resting orders consumed by a match.

| Operation                           | Complexity                                                                                                              |
| ----------------------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| Best bid / ask lookup               | O(1)                                                                                                                    |
| Price-level lookup / insertion      | O(log P)                                                                                                                |
| Active order ID lookup              | Average O(1)                                                                                                            |
| Cancel by ID                        | Average O(1) for the list removal once the iterator is located; O(log P) if an empty price level also has to be removed |
| Quantity modification at same price | Average O(1)                                                                                                            |
| Price modification                  | O(log P), excluding any matching triggered by the new price                                                             |
| Matching                            | Depends on the number of orders consumed and price-level operations                                                     |

These are data-structure-level complexity estimates. Actual runtime also depends on workload, allocation behavior, cache locality, branching, and implementation details.

## Build

Requires a C++17-compatible compiler and CMake.

### Debug build

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
./build-debug/tests
```

### Release build

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

Run the main program:

```bash
./build-release/main
```

Run the benchmark:

```bash
./build-release/benchmark
```

After modifying a source file such as `benchmark.cpp`, the normal workflow is:

```bash
cmake --build build-release
./build-release/benchmark
```

The initial CMake configuration command only needs to be rerun when changing the build configuration or `CMakeLists.txt`.

## Testing

The test suite exercises order matching and order-management behavior, including:

* Full and partial fills
* Multiple price-level matching
* Non-crossing orders
* Order cancellation
* Quantity modification
* Price modification

Tests should verify exact trade details rather than only whether an order remains in the book.

Tests are run in a Debug build so assertions remain enabled.

## Benchmark

The benchmark uses a fixed-seed synthetic workload so that different implementations can be compared under the same order flow.

### Baseline

The recorded baseline is the original implementation using the standard `std::list` allocator.

| Metric                            |                 Baseline |
| --------------------------------- | -----------------------: |
| Orders per run                    |                   10,000 |
| Warmup runs                       |                        3 |
| Measured runs                     |                       20 |
| Total submissions measured        |                  200,000 |
| Total trades across measured runs |                  156,540 |
| Mean runtime per 10,000-order run |               2.24120 ms |
| Median runtime per run            |               2.20821 ms |
| Minimum runtime                   |               2.15467 ms |
| Maximum runtime                   |               2.47554 ms |
| Aggregate throughput              | ~4.46 million orders/sec |

The approximate mean batch time divided by the number of submissions is:

```text
2.24120 ms / 10,000 ≈ 224 ns/order
```

This is an aggregate estimate derived from the batch benchmark, **not a direct measurement of individual order latency**.

Results depend on hardware, compiler, build configuration, workload, and system activity. The benchmark is intended for relative comparison between implementations rather than as a claim of production or exchange-level performance.

## Profiling

The Release-build benchmark was profiled with macOS Instruments / Time Profiler to identify where runtime actually goes rather than relying on assumptions.

### Original implementation

The initial profile of the standard `std::list` implementation showed:

| Call path                       |          Weight |   Self |
| ------------------------------- | --------------: | -----: |
| `OrderBook::addOrder`           |  91.7% (2.23 s) | 434 ms |
| ↳ `OrderBook::matchSellOrder`   |  17.4% (424 ms) |  71 ms |
| ↳↳ Heap allocation/deallocation | ~7.5% (~183 ms) |      — |
| ↳↳ Order-ID hash lookup         |    3.5% (86 ms) |   4 ms |

The allocation-related samples included `operator new`, `_xzm_free`, `_free`, `operator delete`, and `malloc_type_malloc`.

### Initial finding

Approximately **7.5% of total sampled runtime was associated with heap allocation and deallocation**.

This provided measured evidence that the node-based `std::list` design was carrying a meaningful allocation cost. The cost was therefore treated as an optimization target rather than simply assumed from the container's theoretical properties.

This led to the first targeted performance experiment: replacing the list's default allocator with a recycling allocator.

## Recycling allocator experiment

A custom `PoolAllocator` was introduced for the order lists.

The allocator maintains a free list of previously released list-node allocations:

```text
allocate
    │
    ├── recycled node available ──→ reuse
    │
    └── otherwise ────────────────→ operator new
```

When a list node is released, the allocator retains its address for later reuse instead of immediately returning it to the general-purpose allocator.

This implementation is a **recycling allocator**, not yet a fully preallocated contiguous memory pool. When no recycled node is available, it still obtains memory from `operator new`.

### Allocation counters

During the benchmark, the allocator recorded approximately:

```text
Fresh allocations:      2,077
Recycled allocations: 140,799
```

That corresponds to approximately **98.5% of recorded single-node allocation requests being satisfied through the recycling path**.

This demonstrates that the allocator is being exercised heavily and successfully reusing previously released storage.

### Benchmark comparison

Using the same benchmark configuration as the baseline:

| Metric               | Standard `std::list` | Recycling allocator |                    Change |
| -------------------- | -------------------: | ------------------: | ------------------------: |
| Mean                 |           2.24120 ms |      **2.19041 ms** |         **~2.27% faster** |
| Median               |           2.20821 ms |      **2.18777 ms** |         **~0.93% faster** |
| Minimum              |           2.15467 ms |      **1.99071 ms** |          **~7.61% lower** |
| Maximum              |           2.47554 ms |      **3.03750 ms** | **higher in this sample** |
| Aggregate throughput |          4.46189 M/s |     **4.56536 M/s** |         **~2.32% higher** |

### Interpretation

The recycling allocator produced a **small improvement in end-to-end mean runtime and throughput**.

The improvement is considerably smaller than the percentage of allocation requests being recycled because heap allocation is only one part of total order-book execution. Matching logic, tree operations, hash-table operations, list traversal, trade-history storage, branching, and other work remain unchanged.

The result is therefore treated as a measured optimization rather than a claim that memory pooling solved the overall latency problem.

## Re-profiled implementation

After adding the recycling allocator, the benchmark was profiled again.

The profile confirms that the allocator is being used by the list:

```text
std::__1::__list_imp<Order, PoolAllocator<Order>>::__delete_node
```

appears directly in the matching paths.

The list-node deletion path accounted for only about:

```text
10 ms  in matchSellOrder
10 ms  in matchBuyOrder
```

in the profiled run, with no comparable large `free` cost underneath the list-node destruction path. This is consistent with the allocator retaining released nodes for reuse.

At the same time, other parts of the matching engine remained significant sources of runtime.

### Current major hotspots

The re-profiled run showed several substantial contributors:

| Hotspot                                        | Sampled time | Weight |
| ---------------------------------------------- | -----------: | -----: |
| `OrderBook::addOrder`                          |       3.49 s |  91.3% |
| `OrderBook::matchSellOrder`                    |       728 ms |  19.1% |
| `OrderBook::matchBuyOrder`                     |       646 ms |  16.9% |
| `unordered_map` insertion / `__emplace_unique` |       611 ms |  16.0% |
| Red-black tree insertion balancing             |       203 ms |   5.3% |
| `unordered_map` erase                          |       150 ms |   3.9% |
| `operator new`                                 |       276 ms |   7.2% |
| `std::vector<Trade>` insertion                 |        82 ms |   2.1% |
| `std::map` erase                               |       127 ms |   3.3% |

These percentages are profiler samples and should not be added together across nested call paths because parent and child samples overlap.

### Important interpretation

The presence of `operator new` in the second profile does **not** mean that the recycling allocator failed.

The allocator only targets the list's internal node allocations. Other parts of the system continue to use their normal allocators, including:

* `std::map` price-level nodes
* `std::unordered_map` nodes / bucket storage
* `std::vector<Trade>` growth
* the allocator's own `std::vector<T*>` free-list storage
* fresh allocations when the recycling pool has no available node

The re-profile therefore shifted the optimization question from:

> "Is list-node allocation expensive?"

to:

> "Which remaining operations dominate the matching engine after list-node recycling?"

The current profile identifies the active-order `std::unordered_map`, price-level `std::map`, and trade-history vector as important remaining areas to investigate.

## Performance optimization process

The project now follows a measurement-driven optimization loop:

```text
Implement correct data structure
            ↓
Establish benchmark baseline
            ↓
Profile the workload
            ↓
Identify a measured hotspot
            ↓
Apply one targeted change
            ↓
Benchmark against the same baseline
            ↓
Re-profile
            ↓
Identify the next bottleneck
```

The allocator experiment is the first completed iteration of this process.

## Current limitations

* Single-threaded matching
* Synthetic input workload
* No network market-data feed or order gateway
* No persistence or recovery
* No exchange-specific validation or trading rules
* No direct per-order latency measurement
* No P50/P99/P99.9 latency distribution for individual orders
* Recycling allocator is not yet a fully preallocated fixed-capacity pool
* Book-size scaling has not yet been systematically benchmarked
* Current benchmark measures batch runtime and aggregate throughput rather than production-style order latency
* Remaining runtime is still distributed across hash-table, tree, vector, matching, and other operations

## Roadmap

### Completed

* [x] Implement price-time priority matching
* [x] Support full and partial fills
* [x] Implement order cancellation
* [x] Implement quantity and price modification
* [x] Expand edge-case tests and verify exact trade outputs
* [x] Build a repeatable Release benchmark
* [x] Establish a standard `std::list` baseline
* [x] Profile the baseline with macOS Instruments / Time Profiler
* [x] Identify heap allocation as a measurable hotspot
* [x] Implement a recycling allocator for list nodes
* [x] Measure allocator reuse
* [x] Benchmark the allocator against the baseline
* [x] Re-profile the optimized implementation

### Next performance work

* [ ] Investigate `unordered_map` insertion / erase costs
* [ ] Evaluate `reserve()` and rehash behavior for the active-order index
* [ ] Investigate `std::vector<Trade>` growth and reservation
* [ ] Evaluate price-level `std::map` allocation and tree-management costs
* [ ] Implement and benchmark a true preallocated node pool
* [ ] Re-profile after each targeted optimization
* [ ] Measure per-order latency distributions (P50, P95, P99, P99.9)
* [ ] Benchmark sustained workloads
* [ ] Benchmark different book sizes and order-flow patterns
* [ ] Explore alternative order-storage representations, including intrusive or array-based designs

### Systems extensions

* [ ] Build a market-data feed handler
* [ ] Study concurrency and multithreading after establishing single-threaded correctness
* [ ] Study networking and transport considerations
* [ ] Explore separation of order intake and matching components
* [ ] Evaluate latency and architectural tradeoffs introduced by concurrency

## Tech Stack

* C++17
* STL (`map`, `list`, `unordered_map`, `vector`)
* CMake
* `std::chrono`
* `std::random`
* macOS Instruments / Time Profiler
* macOS / AppleClang

## Project Focus

This project is being developed incrementally, prioritizing correctness, data-structure understanding, measurement, and targeted optimization before moving into concurrency and networking.

The performance work is intentionally driven by evidence:

```text
Correctness
    ↓
Data structures
    ↓
Benchmark
    ↓
Profile
    ↓
Optimize
    ↓
Re-profile
    ↓
Repeat
    ↓
Concurrency
    ↓
Networking / system architecture
```

The goal is not to claim production exchange performance. The goal is to build a technically defensible matching engine while demonstrating the ability to reason about data structures, measure real runtime behavior, identify bottlenecks, and evaluate optimizations quantitatively.
