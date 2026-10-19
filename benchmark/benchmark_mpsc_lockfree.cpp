#include "Command.h"
#include "LockFreeMSQueue.h"
#include "OrderBook.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

struct RunTiming
{
    double processingMs;
    double lifecycleMs;
};

double percentile(const std::vector<double>& values, double p)
{
    const std::size_t index =
        static_cast<std::size_t>(p * (values.size() - 1));

    return values[index];
}

RunTiming runOnce(const std::vector<Order>& orders, int numProducers)
{
    OrderBook book(orders.size());

    std::vector<std::thread> producers;
    producers.reserve(numProducers);

    std::atomic<bool> producersDone{false};
    std::atomic<int> rejectedSubmissions{0};

    // Only the consumer modifies this variable.
    std::size_t processed = 0;

    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point processingEnd;
    std::chrono::steady_clock::time_point lifecycleEnd;

    {
        LockFreeMSQueue<OrderCommand> queue;

        // One consumer owns OrderBook mutation.
        std::thread consumer([&]()
        {
            for (;;)
            {
                auto item = queue.tryPop();

                if (item.has_value())
                {
                    auto& addCommand =
                        std::get<AddOrderCommand>(*item);

                    book.addOrder(addCommand.order);
                    ++processed;
                }
                else if (producersDone.load(
                             std::memory_order_acquire))
                {
                    break;
                }
                else
                {
                    // Busy-poll while producers may still publish.
                    std::this_thread::yield();
                }
            }
        });

        // Match the existing MPSC benchmark's timing boundary.
        start = std::chrono::steady_clock::now();

        for (int producerId = 0;
             producerId < numProducers;
             ++producerId)
        {
            producers.emplace_back([&, producerId]()
            {
                for (std::size_t i =
                         static_cast<std::size_t>(producerId);
                     i < orders.size();
                     i += static_cast<std::size_t>(numProducers))
                {
                    OrderCommand command =
                        AddOrderCommand{orders[i]};

                    if (!queue.tryPush(std::move(command)))
                    {
                        rejectedSubmissions.fetch_add(
                            1, std::memory_order_relaxed);
                    }
                }
            });
        }

        // All producers must finish before we signal completion.
        for (auto& producer : producers)
        {
            producer.join();
        }

        producersDone.store(true, std::memory_order_release);

        // Wait for the consumer to drain accepted commands.
        consumer.join();

        // End the processing interval before queue destruction.
        processingEnd = std::chrono::steady_clock::now();
    }

    // LockFreeMSQueue's destructor reclaims retained nodes here.
    lifecycleEnd = std::chrono::steady_clock::now();

    const int rejected = rejectedSubmissions.load(
        std::memory_order_relaxed);

    if (rejected != 0)
    {
        throw std::runtime_error(
            "Some lock-free queue submissions were rejected");
    }

    if (processed != orders.size())
    {
        throw std::runtime_error(
            "Not all submitted orders were processed");
    }

    return {
        std::chrono::duration<double, std::milli>(
            processingEnd - start).count(),

        std::chrono::duration<double, std::milli>(
            lifecycleEnd - start).count()
    };
}

int main()
{
    constexpr int NUM_ORDERS = 10000;
    constexpr int NUM_PRODUCERS = 3;
    constexpr int WARMUP_RUNS = 3;
    constexpr int MEASURED_RUNS = 1000;

    // Controlled workload: all BUY orders at the same price.
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> quantityDist(1, 100);

    std::vector<Order> orders;
    orders.reserve(NUM_ORDERS);

    for (int i = 0; i < NUM_ORDERS; ++i)
    {
        orders.push_back({
            i + 1,
            true,
            100.00,
            quantityDist(rng)
        });
    }

    // Warm-up runs.
    for (int i = 0; i < WARMUP_RUNS; ++i)
    {
        runOnce(orders, NUM_PRODUCERS);
    }

    std::vector<double> processingTimings;
    std::vector<double> lifecycleTimings;

    processingTimings.reserve(MEASURED_RUNS);
    lifecycleTimings.reserve(MEASURED_RUNS);

    // Measured runs.
    for (int i = 0; i < MEASURED_RUNS; ++i)
    {
        const RunTiming timing = runOnce(orders, NUM_PRODUCERS);

        processingTimings.push_back(timing.processingMs);
        lifecycleTimings.push_back(timing.lifecycleMs);
    }

    const double totalProcessingTime = std::accumulate(
        processingTimings.begin(), processingTimings.end(), 0.0);

    const double totalLifecycleTime = std::accumulate(
        lifecycleTimings.begin(), lifecycleTimings.end(), 0.0);

    const double meanProcessing =
        totalProcessingTime / MEASURED_RUNS;

    const double meanLifecycle =
        totalLifecycleTime / MEASURED_RUNS;

    double squaredDifferenceSum = 0.0;

    for (double timing : processingTimings)
    {
        const double difference = timing - meanProcessing;
        squaredDifferenceSum += difference * difference;
    }

    const double standardDeviation = std::sqrt(
        squaredDifferenceSum / (MEASURED_RUNS - 1));

    const double cv =
        standardDeviation / meanProcessing;

    std::vector<double> sortedTimings = processingTimings;
    std::sort(sortedTimings.begin(), sortedTimings.end());

    double median;

    if (MEASURED_RUNS % 2 == 0)
    {
        const int middle = MEASURED_RUNS / 2;

        median = (
            sortedTimings[middle - 1] +
            sortedTimings[middle]) / 2.0;
    }
    else
    {
        median = sortedTimings[MEASURED_RUNS / 2];
    }

    const double p50 = percentile(sortedTimings, 0.50);
    const double p99 = percentile(sortedTimings, 0.99);
    const double p999 = percentile(sortedTimings, 0.999);

    const double minimum = sortedTimings.front();
    const double maximum = sortedTimings.back();

    const long long totalOrders =
        static_cast<long long>(NUM_ORDERS) * MEASURED_RUNS;

    const double processingThroughput =
        totalOrders / (totalProcessingTime / 1000.0);

    const double lifecycleThroughput =
        totalOrders / (totalLifecycleTime / 1000.0);

    std::cout << "\n========== Linked MPSC Benchmark ==========\n";
    std::cout << "Producers: " << NUM_PRODUCERS << '\n';
    std::cout << "Orders per run: " << NUM_ORDERS << '\n';
    std::cout << "Warm-up runs: " << WARMUP_RUNS << '\n';
    std::cout << "Measured runs: " << MEASURED_RUNS << '\n';
    std::cout << "Total orders: " << totalOrders << '\n';

    std::cout << "\n--- Processing interval (cleanup excluded) ---\n";
    std::cout << "Mean: " << meanProcessing << " ms\n";
    std::cout << "Median: " << median << " ms\n";
    std::cout << "P50: " << p50 << " ms\n";
    std::cout << "P99: " << p99 << " ms\n";
    std::cout << "P99.9: " << p999 << " ms\n";
    std::cout << "Min: " << minimum << " ms\n";
    std::cout << "Max: " << maximum << " ms\n";
    std::cout << "Standard deviation: "
              << standardDeviation << " ms\n";
    std::cout << "CV: " << cv * 100.0 << "%\n";
    std::cout << "Processing throughput: "
              << processingThroughput << " orders/s\n";

    std::cout << "\n--- Full lifecycle (cleanup included) ---\n";
    std::cout << "Mean: " << meanLifecycle << " ms\n";
    std::cout << "Lifecycle throughput: "
              << lifecycleThroughput << " orders/s\n";

    return 0;
}