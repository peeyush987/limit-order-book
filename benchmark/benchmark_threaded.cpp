#include "MatchingEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

double percentile(const std::vector<double>& values, double p)
{
    size_t index =
        static_cast<size_t>(p * (values.size() - 1));

    return values[index];
}

double runOnce(const std::vector<Order>& orders)
{
    MatchingEngine engine;
    engine.start();

    auto start = std::chrono::steady_clock::now();

    for (const auto& order : orders)
    {
        bool accepted = engine.submit(AddOrderCommand{order});

        if (!accepted)
        {
            throw std::runtime_error(
                "Order submission rejected during benchmark"
            );
        }
    }

    // Close queue, drain accepted commands, and join worker.
    engine.stop();

    auto end = std::chrono::steady_clock::now();

    return std::chrono::duration<double, std::milli>(
        end - start
    ).count();
}

int main()
{
    constexpr int NUM_ORDERS = 10000;
    constexpr int WARMUP_RUNS = 3;
    constexpr int MEASURED_RUNS = 1000;

    // Generate the same deterministic workload as the baseline.
    std::mt19937 rng(42);

    std::uniform_int_distribution<int> priceTickDist(9000, 11000);
    std::uniform_int_distribution<int> quantityDist(1, 100);
    std::uniform_int_distribution<int> sideDist(0, 1);

    std::vector<Order> orders;
    orders.reserve(NUM_ORDERS);

    for (int i = 0; i < NUM_ORDERS; ++i)
    {
        bool isBuy = sideDist(rng) == 1;
        double price = priceTickDist(rng) / 100.0;
        int quantity = quantityDist(rng);

        orders.push_back({
            i + 1,
            isBuy,
            price,
            quantity
        });
    }

    // Warm-up runs: not measured.
    for (int i = 0; i < WARMUP_RUNS; ++i)
    {
        runOnce(orders);
    }

    std::vector<double> timings;
    timings.reserve(MEASURED_RUNS);

    for (int i = 0; i < MEASURED_RUNS; ++i)
    {
        timings.push_back(runOnce(orders));
    }

        // Total measured time across all runs.
    double totalTime =
        std::accumulate(timings.begin(), timings.end(), 0.0);

    // Mean.
    double mean = totalTime / MEASURED_RUNS;

    // Standard deviation.
    double squaredDifferenceSum = 0.0;

    for (double timing : timings)
    {
        double difference = timing - mean;
        squaredDifferenceSum += difference * difference;
    }

    double standardDeviation =
        std::sqrt(squaredDifferenceSum / (MEASURED_RUNS - 1));

    // Coefficient of variation.
    double coefficientOfVariation = standardDeviation / mean;

    // Sort for percentile and min/max calculations.
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

    // Overall throughput, based on summed measured time.
    long long totalOrders =
        static_cast<long long>(NUM_ORDERS) * MEASURED_RUNS;

    double totalSeconds = totalTime / 1000.0;
    double overallThroughput = totalOrders / totalSeconds;

    std::cout << "\n========== Threaded Benchmark Summary ==========\n";
    std::cout << "Orders per run: " << NUM_ORDERS << '\n';
    std::cout << "Warm-up runs: " << WARMUP_RUNS << '\n';
    std::cout << "Measured runs: " << MEASURED_RUNS << '\n';
    std::cout << "Total orders submitted: " << totalOrders << '\n';

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
}
