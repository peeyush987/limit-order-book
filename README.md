# Limit Order Book & Concurrent Matching Engine

A C++17 limit order book and matching engine with price-time priority, full and partial fills, cancellation, modification, trade history, a thread-safe API, and a producer-consumer command pipeline. Development was measurement-driven: correctness first, then profiling and targeted optimization, then concurrency, with each design decision validated against repeatable benchmarks.

## Features

- **Price-time priority matching** with full and partial fills
- **Order management:** cancellation, quantity modification, and price modification
- **O(1) average order lookup** by ID via an active-order index
- **Trade history** recording buyer ID, seller ID, execution price, and quantity
- **Thread-safe OrderBook** protected by a coarse-grained mutex
- **Producer-consumer pipeline:** multiple producers submit commands (ADD, CANCEL, MODIFY) to a dedicated matching thread
- **Queue experiment:** mutex-based MPSC queue vs. an experimental linked, CAS-based MPSC queue
- **Performance tooling:** fixed-seed benchmarks and macOS Instruments / Time Profiler analysis
- **Automated tests** covering matching, order management, and concurrency

## Architecture

### Matching rules

1. A BUY order checks the lowest ask; a SELL order checks the highest bid.
2. Matching continues while the best opposing price crosses the incoming order's limit.
3. Executed quantity is the smaller of the incoming and resting quantities.
4. Trades execute at the **resting order's price**.
5. Fully filled resting orders are removed from their price level and the active-order index.
6. Any remaining incoming quantity rests in the book.

### Data structures

| Component | Purpose | Rationale |
|---|---|---|
| `std::map` (bids) | Price levels in descending order | Best bid at `begin()` |
| `std::map` (asks) | Price levels in ascending order | Best ask at `begin()` |
| `std::list<Order, PoolAllocator<Order>>` | FIFO orders within a price level | Preserves time priority; O(1) removal with a stored iterator |
| `std::unordered_map` | Order ID → side, price, list iterator | Avoids scanning the book for cancels and modifications |
| `std::vector<Trade>` | Executed trades | Contiguous storage with efficient appends |

A list is used rather than a queue because cancellation can target an order in the middle of a price level. The list allocator is a **recycling allocator**: it reuses released node storage and falls back to `operator new` when its free list is empty. It is not a preallocated contiguous pool.

### Concurrency

The OrderBook's shared mutable state (bids, asks, the order index, and trade history) is protected by a coarse-grained mutex. Public operations lock around a complete logical state transition; internal `_Unlocked` helpers assume the caller already holds the lock.

```text
Producer threads
      |
      v
CommandQueue
      |
      v
Dedicated MatchingEngine thread
      |
      v
OrderBook (serialized state transitions)
```

Multiple producers submit concurrently while one consumer mutates the book serially. This defines a clear correctness boundary; matching itself is not parallelized.

### Complexity

Let `P` be the number of price levels.

| Operation | Complexity |
|---|---|
| Best bid / ask lookup | O(1) |
| Price-level lookup / insertion | O(log P) |
| Order lookup by ID | Average O(1) |
| Cancel by ID | Average O(1); O(log P) if an empty price level must be erased |
| Quantity modification (same price) | Average O(1) |
| Price modification | O(log P), excluding matching triggered by the new price |
| Matching | Proportional to orders consumed plus price-level operations |

These are data-structure-level estimates; real runtime also depends on allocation, cache locality, branching, and workload.

## Build and Test

Requires a C++17 compiler and CMake.

**Debug build and tests** (assertions enabled):

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
./build-debug/tests
```

**Release build** (benchmarks use an explicit `-O2 -DNDEBUG` configuration):

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG"
cmake --build build-release

./build-release/main
./build-release/benchmark
```

The threaded pipeline and queue-comparison benchmarks have their own targets: build with `cmake --build build-release --target <target-name>` and run the matching executable under `build-release/`.

## Testing

The suite verifies exact trade details, not just book state, and covers:

- Full and partial fills, multiple price levels, and non-crossing orders
- Cancellation, quantity modification, and price modification
- Concurrent insertion, cancellation, and mixed reads/writes
- Producer-consumer command processing and MPSC queue paths

Sanitizer runs reported no data races in the tested paths. This raises confidence in the exercised behavior but is not a proof of correctness for every possible interleaving.

## Benchmarks

### Single-threaded methodology

| Setting | Value |
|---|---:|
| Orders per run | 10,000 |
| Warm-up runs | 3 |
| Measured runs | 1,000 |
| Random seed | 42 |
| Price range / tick | 90.00–110.00 / 0.01 |
| Quantity | Random integer, 1–100 |
| Side | Random BUY or SELL |

The workload is generated once from a fixed seed and reused for every run. Prices are generated as integer ticks so arbitrary floating-point values don't create a distinct price level for nearly every order. Reported percentiles describe the runtime of a full 10,000-order batch, **not** per-order latency.

### Optimization results

The baseline used the original implementation: standard `std::list` allocator, no `reserve()` calls, and a temporary per-order `std::vector<Trade>`. Results across three invocations of the 1,000-run benchmark:

| Metric | Baseline | Optimized |
|---|---:|---:|
| Mean runtime per batch | 2.098–2.285 ms | 1.191–1.240 ms |
| Mean of invocation means | 2.161 ms | 1.220 ms |
| Aggregate throughput | 4.38–4.77M orders/s | 8.06–8.40M orders/s |
| Trades per run | 7,750 | 7,750 |

Mean batch runtime dropped by roughly **43.5%** and throughput rose roughly **1.77×**. The identical trade count confirms the speedup did not come from doing less work. Optimized tail statistics (P99 1.248–1.782 ms, P99.9 1.367–4.264 ms, CV 1.31–16.66%) reflect desktop scheduling noise on short measurements.

### Profiling-driven changes

macOS Instruments / Time Profiler guided each change:

1. **Recycling allocator for list nodes.** Allocation/deallocation was prominent in the initial profile. The end-to-end gain was modest, showing allocation was only one contributor.
2. **`orderMap.reserve(...)`.** Hash-table insertion was a major hotspot; pre-sizing avoids repeated growth and rehashing, and sampled time in that path dropped afterward.
3. **`tradeHistory.reserve(...)`.** Avoids repeated vector growth while appending thousands of trades per run.
4. **Direct writes to `tradeHistory`.** Removes the temporary per-order `std::vector<Trade>` and its copy/insertion cost.

The frozen tick-based workload is a methodology improvement, not a speedup.

The final profile still shows time in `matchBuyOrder` (~15.7%), `matchSellOrder` (~14.2%), hash-table insertion (~15.4%), red-black-tree balancing (~6.6%), and `operator new` (~8.2%). Percentages are approximate, and parent/child call paths overlap, so they should not be summed. Optimization stopped at a defensible, measured improvement rather than continuing without a clear expected payoff.

### Threaded command pipeline

Three producer threads submit to a dedicated matching thread. The measurement covers the full submission-to-drain interval, including queue coordination and shutdown.

| Metric | Result |
|---|---:|
| Orders per run / measured runs | 10,000 / 1,000 |
| Mean batch time | 2.150 ms |
| Median / P99 / P99.9 | 2.144 / 2.296 / 2.817 ms |
| Min / Max | 1.965 / 3.142 ms |
| Std. deviation (CV) | 0.057 ms (2.66%) |
| Throughput | ~4.65M orders/s |

This is **not** an apples-to-apples comparison with the single-threaded benchmark, which times direct OrderBook calls. The threaded design establishes a concurrency architecture and correctness boundary; it is not claimed to be faster than direct calls.

### MPSC queue comparison

Two queue designs were compared with the same harness:

- **Mutex polling queue:** mutex-protected queue, multiple producers, one consumer.
- **Linked MS-style queue:** experimental linked queue inspired by Michael–Scott, using atomics/CAS.

Workload: three producers, one consumer, 50,000 orders per run, 3 warm-up runs, 1,000 measured runs. Orders were BUY orders at a fixed price with random quantities, so the test measures command ingestion rather than trade-heavy matching.

| Metric | Mutex polling | Linked MS-style |
|---|---:|---:|
| Mean processing time per batch | 7.498 ms | 10.988 ms |
| Median processing time | 6.418 ms | 9.668 ms |
| P99 / P99.9 | 20.579 / 108.486 ms | 21.302 / 48.849 ms |
| Min / Max | 5.548 / 155.897 ms | 7.901 / 94.333 ms |
| Std. deviation (CV) | 6.252 ms (83.39%) | 3.807 ms (34.65%) |
| Processing throughput | 6.669M orders/s | 4.550M orders/s |
| Full-lifecycle throughput | 6.668M orders/s | 4.157M orders/s |

The linked queue's mean processing time was ~46.5% higher and its throughput ~31.8% lower (~37.7% lower over the full lifecycle). Both showed high variability, especially the mutex queue, which points to desktop scheduling effects.

**Decision:** the mutex polling queue is the production path. It delivered higher throughput, has a simpler synchronization and ownership model, and avoids per-command node allocation and deferred cleanup. The linked queue remains an experiment: consumed nodes are retained until destruction and safe memory reclamation is not implemented. The result applies to this implementation and workload; it is not a general claim that mutex queues beat lock-free designs.

## Limitations

- Matching and OrderBook state transitions are serialized; matching is not parallelized.
- The workload is synthetic and does not model realistic order flow (clustered prices, cancellation distributions, bursts).
- Prices are stored as `double`, even though they are generated in discrete ticks.
- Reported percentiles are batch runtimes, not per-order latencies.
- The recycling allocator can still call `operator new` and is not a fixed-capacity pool.
- The linked queue experiment lacks safe memory reclamation and is not used in production.
- Book-size scaling has not been systematically benchmarked.
- Network feed transport, persistence, and recovery are out of scope.

## Technology

C++17 (STL containers, atomics, mutexes, condition variables, threads), CMake, `std::chrono`, `std::random`, macOS / AppleClang, macOS Instruments / Time Profiler.

## Summary

The project delivers a tested, thread-safe price-time priority matching core and a producer-consumer engine architecture. Profiling-guided changes to allocation, hash-table growth, vector capacity, and trade recording cut mean batch runtime from ~2.161 ms to ~1.220 ms (~1.77× throughput) on a fixed workload. The queue experiment showed that a more sophisticated lock-free design is not automatically faster: in the tested three-producer workload, the simpler mutex queue won. All results are documented with their workload definitions and limitations rather than presented as universal performance claims.