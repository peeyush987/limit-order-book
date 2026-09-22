Limit Order Book & Matching Engine

A C++17 implementation of a single-threaded limit order book with price-time priority matching, order cancellation, modification, trade history, automated tests, repeatable benchmarking, profiling, and measurement-driven performance optimization.

Overview

This project models the core order-matching logic of an electronic trading system. It maintains buy and sell orders, matches incoming orders against the best available prices, and preserves FIFO priority among orders at the same price level.

The project is being developed incrementally: correctness and data-structure understanding come first, followed by profiling, targeted optimization, and eventually concurrency and networking.

The performance work is intentionally measurement-driven. Changes are made only after establishing a benchmark and identifying a concrete runtime cost to investigate.

Features

Price-time priority: Best price first, then FIFO within each price level.

Order matching: Supports full and partial fills.

Order cancellation: Removes active orders by ID.

Order modification: Supports quantity changes and price changes.

Order lookup: Uses an ID index to locate active orders without scanning the book.

Trade history: Records executed trades with buyer ID, seller ID, execution price, and quantity.

Automated tests: Covers matching and order-management behavior.

Benchmarking: Uses a fixed-seed synthetic workload for repeatable measurements.

Profiling: Uses macOS Instruments / Time Profiler to identify actual runtime hotspots.

Memory optimization: Uses a recycling allocator for std::list nodes and reserves capacity for major containers.

Architecture

Data structures

Component

Purpose

std::map for bids

Maintains bid price levels in descending order

std::map for asks

Maintains ask price levels in ascending order

std::list<Order, PoolAllocator<Order>>

Stores FIFO orders within each price level

std::unordered_map

Maps active order IDs to their side, price, and list iterator

std::vector<Trade>

Stores executed trades

The order ID index allows an active order to be located without scanning every price level.

std::list is used instead of std::queue because cancellation may target an order anywhere within a price level. Given a valid iterator, removing a list element is O(1).

The list allocator was replaced with a recycling allocator so released list-node storage can be reused by later allocations. The allocator is intentionally a recycling allocator rather than a fully preallocated contiguous memory pool: when no recycled node is available, it still obtains memory from operator new.

Matching rules

A BUY order checks the lowest ask.

A SELL order checks the highest bid.

Matching continues while prices cross.

The executed quantity is the smaller of the incoming and resting quantities.

The resting order's price is used as the execution price.

Fully filled resting orders are removed from the book and active-order index.

Any remaining incoming quantity is added to the book.

Complexity

Let P be the number of price levels and K the number of resting orders consumed by a match.

Operation

Complexity

Best bid / ask lookup

O(1)

Price-level lookup / insertion

O(log P)

Active order ID lookup

Average O(1)

Cancel by ID

Average O(1) for ID lookup and list removal; O(log P) if an empty price level also has to be removed

Quantity modification at same price

Average O(1)

Price modification

O(log P), excluding any matching triggered by the new price

Matching

Depends on the number of orders consumed and price-level operations

These are data-structure-level complexity estimates. Actual runtime also depends on allocation behavior, cache locality, branching, workload, compiler, and implementation details.

Build

Requires a C++17-compatible compiler and CMake.

Debug build

cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
./build-debug/tests

Release build

The benchmark experiments use an explicit -O2 -DNDEBUG Release configuration for reproducibility:

rm -rf build-release
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG"
cmake --build build-release

Run the main program:

./build-release/main

Run the benchmark:

./build-release/benchmark

After modifying a source file without changing the build configuration, the normal workflow is:

cmake --build build-release
./build-release/benchmark

Testing

The test suite exercises order matching and order-management behavior, including:

Full and partial fills

Multiple price-level matching

Non-crossing orders

Order cancellation

Quantity modification

Price modification

Tests verify exact trade details rather than only whether an order remains in the book.

Tests are run in a Debug build so assertions remain enabled.

Benchmark

The benchmark uses a fixed random seed and a synthetic order flow so implementations can be compared under the same generated workload.

Current benchmark workload

Orders per run: 10,000

Measured runs: 1,000

Price range: 90.00 to 110.00

Price tick: 0.01

Warmup runs: 3

Random seed: 42

Prices are generated as integer ticks and converted to the current double price representation. This avoids arbitrary floating-point prices creating a different std::map price level for nearly every order.

Current clean baseline

The current clean benchmark includes:

PoolAllocator for list nodes

orderMap.reserve(NUM_ORDERS)

tradeHistory.reserve(NUM_ORDERS)

Direct tradeHistory.push_back(...) from the matching functions, removing the temporary per-order std::vector<Trade>

Three separate invocations of the 1,000-run benchmark produced the following ranges:

Metric

Observed range across 3 invocations

Mean runtime per 10,000-order run

1.191–1.240 ms

P50

1.166–1.208 ms

P99

1.248–1.782 ms

P99.9

1.367–4.264 ms

Maximum

1.368–5.582 ms

CV

1.31%–16.66%

Aggregate throughput

8.06–8.40 million orders/sec

Trades per benchmark run

7,750

Across the three invocations, the arithmetic mean of the run-level mean runtimes was approximately 1.22 ms per 10,000-order batch.

These are batch-level measurements, not direct per-order latency measurements. Run-to-run tail variation is expected on a general-purpose desktop OS because scheduling and other system activity can affect wall-clock measurements.

Historical standard-allocation baseline

Before the recycling allocator and later optimizations, the original benchmark used arbitrary floating-point prices and 20 measured runs:

Metric

Original baseline

Orders per run

10,000

Warmup runs

3

Measured runs

20

Total submissions measured

200,000

Total trades across measured runs

156,540

Mean runtime per 10,000-order run

2.24120 ms

Median runtime per run

2.20821 ms

Minimum runtime

2.15467 ms

Maximum runtime

2.47554 ms

Aggregate throughput

~4.46 million orders/sec

The old result is retained as a historical reference rather than the current benchmark baseline because the workload definition has since been improved to use discrete price ticks.

Profiling

The Release-build benchmark is profiled with macOS Instruments / Time Profiler to identify where runtime actually goes rather than relying on assumptions.

Initial profiling findings

The original implementation's profile showed substantial time associated with heap allocation and deallocation in the matching path. This motivated the recycling-allocator experiment.

A later profile of the allocator-enabled implementation showed several meaningful contributors, including:

Hotspot

Approx. sampled weight in the profiled run

OrderBook::matchSellOrder()

19.1%

OrderBook::matchBuyOrder()

16.9%

unordered_map insertion / __emplace_unique

16.0%

Red-black tree insertion balancing

5.3%

unordered_map erase

3.9%

operator new

7.2%

std::vector<Trade> insertion

2.1%

std::map erase

3.3%

Profiler percentages represent sampled CPU time and parent/child call paths overlap, so the percentages should not be summed as independent costs.

The profile demonstrated that list-node allocation was only one part of the runtime. Other work in the active-order hash table, price-level tree, matching logic, trade-history storage, and general allocation remained significant.

Performance optimization history

1. Recycling allocator for list nodes

The order lists use a custom PoolAllocator that retains released list-node addresses for reuse.

Conceptually:

allocate
   │
   ├── recycled node available ──→ reuse
   │
   └── otherwise ────────────────→ operator new

The allocator's measured reuse rate was very high on the earlier workload. Despite that, the end-to-end benchmark improvement was relatively small, demonstrating that a high number of recycled allocations does not automatically make allocation the dominant system bottleneck.

2. Reserve capacity for the active-order index

orderMap.reserve(expectedOrders) was added after profiling showed substantial time in unordered_map insertion.

reserve() prepares bucket capacity in advance so the hash table does not have to repeatedly grow and rehash as entries are inserted.

The profile after this change showed a substantial reduction in sampled time attributed to the hash-table insertion path, supporting the original rehash/growth hypothesis.

3. Reserve trade-history capacity

tradeHistory.reserve(expectedOrders) was added because the benchmark generates thousands of trades per run. The goal is to reduce repeated vector-capacity growth as trade records are appended.

4. Remove the temporary per-order trade vector

The matching functions previously built a temporary std::vector<Trade> and then inserted its contents into tradeHistory.

The current implementation writes trades directly to the final history vector:

Previous:
match → temporary vector<Trade> → tradeHistory

Current:
match ────────────────────────→ tradeHistory

This removes an intermediate storage path and associated copying/insertion work. The benchmark showed a large end-to-end improvement after this change while preserving the same total trade count for the benchmark workload.

5. Discrete price ticks in the benchmark

The benchmark previously generated arbitrary floating-point prices. That could create a very large number of distinct std::map price levels even inside the relatively narrow 90–110 price range.

The benchmark now uses a 0.01 tick size:

90.00
90.01
90.02
...
110.00

This makes the synthetic workload easier to reason about and better aligned with the idea of discrete market price levels. It is a benchmark-design change, so current tick-based results are not directly compared numerically with the earlier arbitrary-price results.

Current limitations

Single-threaded matching

Synthetic input workload

Current price representation still uses double

No network market-data feed or order gateway

No persistence or recovery

No exchange-specific validation or trading rules

No direct per-order latency measurement

P50/P99/P99.9 currently describe batch runtime distributions, not individual order latency

Recycling allocator is not yet a fully preallocated fixed-capacity pool

Book-size scaling has not yet been systematically benchmarked

Current benchmark does not model realistic order-flow distributions such as clustered price levels, cancellations, bursts, or time-varying activity

Roadmap

Completed

Implement price-time priority matching

Support full and partial fills

Implement order cancellation

Implement quantity and price modification

Expand edge-case tests and verify exact trade outputs

Build a repeatable Release benchmark

Establish a standard-allocation baseline

Profile the baseline with macOS Instruments / Time Profiler

Implement and benchmark a recycling allocator for list nodes

Re-profile the allocator-enabled implementation

Add orderMap.reserve(...)

Add tradeHistory.reserve(...)

Remove the temporary per-order std::vector<Trade>

Add P50 / P99 / P99.9 batch-runtime measurements

Add coefficient-of-variation reporting

Define a discrete 0.01 benchmark price tick

Next performance work

Profile the current tick-based workload

Investigate remaining unordered_map insertion / erase costs

Investigate std::map price-level allocation and tree-management costs

Compare alternative price-level representations under the same tick-based workload

Benchmark different book sizes and order-flow patterns

Benchmark sustained workloads

Re-profile after each targeted optimization

Measure individual-order latency distributions (P50, P95, P99, P99.9) with a measurement method that minimizes instrumentation overhead

Explore more efficient order-storage representations, including intrusive or array-based designs

Evaluate a true preallocated fixed-capacity node pool

Systems extensions

Build a market-data feed handler

Study concurrency and multithreading after establishing single-threaded correctness

Study networking and transport considerations

Explore separation of order intake and matching components

Evaluate latency and architectural tradeoffs introduced by concurrency

Tech Stack

C++17

STL (map, list, unordered_map, vector)

CMake

std::chrono

std::random

macOS Instruments / Time Profiler

macOS / AppleClang

Project Focus

This project is being developed incrementally, prioritizing correctness, data-structure understanding, measurement, and targeted optimization before moving into concurrency and networking.

The performance workflow is:

Correctness
    ↓
Data structures
    ↓
Benchmark
    ↓
Profile
    ↓
Identify a measured hotspot
    ↓
Apply one targeted change
    ↓
Benchmark against the same workload
    ↓
Re-profile
    ↓
Repeat
    ↓
Concurrency
    ↓
Networking / system architecture

The goal is not to claim production exchange performance. The goal is to build a technically defensible matching engine while demonstrating the ability to reason about data structures, measure real runtime behavior, identify bottlenecks, and evaluate optimizations quantitatively.