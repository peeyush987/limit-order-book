
#include "OrderBook.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

int main()
{
    constexpr int NUM_ORDERS = 10000;
    constexpr int WARMUP_RUNS = 3;
    constexpr int MEASURED_RUNS = 1000;

    // Fixed seed -> same workload every time.
    std::mt19937 rng(42);

    std::uniform_int_distribution<int> priceTickDist(9000, 11000);
    std::uniform_int_distribution<int> quantityDist(1, 100);
    std::uniform_int_distribution<int> sideDist(0, 1);

    // Generate workload once.
    std::vector<Order> orders;
    orders.reserve(NUM_ORDERS);

    for (int i = 0; i < NUM_ORDERS; ++i)
    {
        bool isBuy = sideDist(rng) == 1;
        double price = priceTickDist(rng)/100.0;
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
        OrderBook ob;

        for (const auto& order : orders)
        {
            ob.addOrder(order);
        }
    }

    std::vector<double> timings;
    timings.reserve(MEASURED_RUNS);

    size_t totalTrades = 0;

    // Measured runs.
    for (int run = 0; run < MEASURED_RUNS; ++run)
    {
        OrderBook ob;

        auto start = std::chrono::steady_clock::now();

        for (const auto& order : orders)
        {
            ob.addOrder(order);
        }

        auto end = std::chrono::steady_clock::now();

        double milliseconds =
            std::chrono::duration<double, std::milli>(
                end - start
            ).count();

        timings.push_back(milliseconds);
        totalTrades += ob.getTradeHistory().size();

        double seconds = milliseconds / 1000.0;
        double throughput = NUM_ORDERS / seconds;

        std::cout << "Run " << run + 1
                  << ": " << milliseconds << " ms"
                  << " | Throughput: " << throughput
                  << " orders/s\n";
    }

    // Total elapsed time across all measured runs.
    double totalTime =
        std::accumulate(timings.begin(), timings.end(), 0.0);

    // Mean.
    double mean = totalTime / MEASURED_RUNS;

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

    // Overall throughput across all measured runs.
    int totalOrders = NUM_ORDERS * MEASURED_RUNS;
    double totalSeconds = totalTime / 1000.0;
    double overallThroughput = totalOrders / totalSeconds;

    std::cout << "\n========== Benchmark Summary ==========\n";

    std::cout << "Orders per run: " << NUM_ORDERS << '\n';
    std::cout << "Measured runs: " << MEASURED_RUNS << '\n';
    std::cout << "Total orders: " << totalOrders << '\n';
    std::cout << "Total trades: " << totalTrades << '\n';

    std::cout << "Mean: " << mean << " ms\n";
    std::cout << "Median: " << median << " ms\n";
    std::cout << "Min: " << minimum << " ms\n";
    std::cout << "Max: " << maximum << " ms\n";

    std::cout << "Overall throughput: "
              << overallThroughput
              << " orders/s\n";

    return 0;
}
