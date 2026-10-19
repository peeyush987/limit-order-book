#include "MatchingEngine.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

double percentile(const std::vector<double>& values, double p)
{
    size_t index =
        static_cast<size_t>(p * (values.size() - 1));

    return values[index];
}

// Run one complete benchmark trial using multiple producers.
double runOnce(const std::vector<Order>& orders, int numProducers)
{
    MatchingEngine engine;
    engine.start();

    std::atomic<int> rejectedSubmissions{0};
    std::vector<std::thread> producers;
    producers.reserve(numProducers);

    const int numOrders = static_cast<int>(orders.size());

    // Start timing after engine construction and thread startup.
    auto start = std::chrono::steady_clock::now();

    for (int producerId = 0;
         producerId < numProducers;
         ++producerId)
    {
        producers.emplace_back([&, producerId]()
        {
            // Each producer handles a distinct subset of the workload.
            for (int i = producerId;
                 i < numOrders;
                 i += numProducers)
            {
                bool accepted =
                    engine.submit(AddOrderCommand{orders[i]});

                if (!accepted)
                {
                    rejectedSubmissions.fetch_add(1);
                }
            }
        });
    }

    // Wait until every producer has finished submitting.
    for (auto& producer : producers)
    {
        producer.join();
    }

    int rejected = rejectedSubmissions.load();

    // Close the queue, drain accepted commands, and join the
    // matching thread before stopping the timer.
    engine.stop();

    auto end = std::chrono::steady_clock::now();

    if (rejected != 0)
    {
        throw std::runtime_error(
            "Some order submissions were rejected"
        );
    }

    return std::chrono::duration<double, std::milli>(
        end - start
    ).count();
}

int main()
{
    constexpr int NUM_ORDERS = 10000;
    constexpr int NUM_PRODUCERS = 3;
    constexpr int WARMUP_RUNS = 3;
    constexpr int MEASURED_RUNS = 1000;

    // Use the same deterministic workload generation as the baseline.
    std::mt19937 rng(42);

    std::uniform_int_distribution<int> priceTickDist(9000, 11000);
    std::uniform_int_distribution<int> quantityDist(1, 100);
    std::uniform_int_distribution<int> sideDist(0, 1);

    std::vector<Order> orders;
    orders.reserve(NUM_ORDERS);

    for (int i = 0; i < NUM_ORDERS; ++i)
    {
        int quantity = quantityDist(rng);

    orders.push_back({
        i + 1,
        true,       // Every order is a BUY
        100.00,     // Every order uses the same price
        quantity
    });
    }

    // Warm-up runs: execute, but do not record their timings.
    for (int i = 0; i < WARMUP_RUNS; ++i)
    {
        runOnce(orders, NUM_PRODUCERS);
    }

    // Collect measured runs.
    std::vector<double> timings;
    timings.reserve(MEASURED_RUNS);

    for (int i = 0; i < MEASURED_RUNS; ++i)
    {
        timings.push_back(runOnce(orders, NUM_PRODUCERS));
    }

    // Calculate total time and mean.
    double totalTime =
        std::accumulate(timings.begin(), timings.end(), 0.0);

    double mean = totalTime / MEASURED_RUNS;

    // Calculate sample standard deviation.
    double squaredDifferenceSum = 0.0;

    for (double timing : timings)
    {
        double difference = timing - mean;
        squaredDifferenceSum += difference * difference;
    }

    double standardDeviation =
        std::sqrt(squaredDifferenceSum / (MEASURED_RUNS - 1));

    double coefficientOfVariation = standardDeviation / mean;

    // Sort a copy for percentile and min/max calculations.
    std::vector<double> sortedTimings = timings;
    std::sort(sortedTimings.begin(), sortedTimings.end());

    double median;

    if (MEASURED_RUNS % 2 == 0)
    {
        int middle = MEASURED_RUNS / 2;

        median =
            (sortedTimings[middle - 1] +
             sortedTimings[middle]) / 2.0;
    }
    else
    {
        median = sortedTimings[MEASURED_RUNS / 2];
    }

    double minimum = sortedTimings.front();
    double maximum = sortedTimings.back();

    double p50 = percentile(sortedTimings, 0.50);
    double p99 = percentile(sortedTimings, 0.99);
    double p999 = percentile(sortedTimings, 0.999);

    // Overall throughput across measured runs.
    long long totalOrders =
        static_cast<long long>(NUM_ORDERS) * MEASURED_RUNS;

    double totalSeconds = totalTime / 1000.0;
    double overallThroughput = totalOrders / totalSeconds;

    // Print summary.
    std::cout << "\n========== MPSC Benchmark Summary ==========\n";
    std::cout << "Producers: " << NUM_PRODUCERS << '\n';
    std::cout << "Orders per run: " << NUM_ORDERS << '\n';
    std::cout << "Warm-up runs: " << WARMUP_RUNS << '\n';
    std::cout << "Measured runs: " << MEASURED_RUNS << '\n';
    std::cout << "Total order submissions: " << totalOrders << '\n';

    std::cout << "Mean: " << mean << " ms\n";
    std::cout << "Median: " << median << " ms\n";
    std::cout << "P50: " << p50 << " ms\n";
    std::cout << "P99: " << p99 << " ms\n";
    std::cout << "P99.9: " << p999 << " ms\n";
    std::cout << "Min: " << minimum << " ms\n";
    std::cout << "Max: " << maximum << " ms\n";

    std::cout << "Standard deviation: "
              << standardDeviation << " ms\n";

    std::cout << "CV: "
              << coefficientOfVariation * 100.0 << "%\n";

    std::cout << "Overall throughput: "
              << overallThroughput << " orders/s\n";

    return 0;
}