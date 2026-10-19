
#include "Command.h"
#include "LockFreeMSQueue.h"
#include "OrderBook.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

// Mutex-protected queue with the same tryPush/tryPop interface.
template <typename T>
class MutexPollingMPSCQueue
{
private:
    std::mutex mutex_;
    std::queue<T> queue_;

public:
    bool tryPush(T&& value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        return true;
    }

    std::optional<T> tryPop()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty())
        {
            return std::nullopt;
        }

        T value = std::move(queue_.front());
        queue_.pop();

        return std::optional<T>{std::move(value)};
    }
};

struct RunTiming
{
    double processingMs;
    double lifecycleMs;
};

// Both queue implementations use this exact same function.
template <typename Queue>
RunTiming runOnce(
    const std::vector<Order>& orders,
    int numProducers)
{
    // Construct the book outside the timed interval.
    OrderBook book(orders.size());

    std::atomic<bool> producersDone{false};
    std::atomic<int> rejectedSubmissions{0};

    std::size_t processed = 0;

    using Clock = std::chrono::steady_clock;

    Clock::time_point start;
    Clock::time_point processingEnd;
    Clock::time_point lifecycleEnd;

    {
        Queue queue;

        std::vector<std::thread> producers;
        producers.reserve(numProducers);

        // Exactly one consumer modifies the OrderBook.
        std::thread consumer([&]()
        {
            for (;;)
            {
                auto item = queue.tryPop();

                if (item.has_value())
                {
                    const auto& add =
                        std::get<AddOrderCommand>(*item);

                    book.addOrder(add.order);
                    ++processed;
                }
                else if (producersDone.load(
                             std::memory_order_acquire))
                {
                    break;
                }
                else
                {
                    // Same idle polling strategy for both queues.
                    std::this_thread::yield();
                }
            }
        });

        // Consumer is started before timing begins.
        start = Clock::now();

        // Distribute the same workload across all producers.
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

        // Wait until all producers have finished submitting.
        for (auto& producer : producers)
        {
            producer.join();
        }

        producersDone.store(true, std::memory_order_release);

        // Wait until the consumer has drained accepted commands.
        consumer.join();

        // Processing is complete. Queue destruction isn't included
        // in this timestamp.
        processingEnd = Clock::now();

        if (rejectedSubmissions.load(
                std::memory_order_relaxed) != 0)
        {
            throw std::runtime_error(
                "Some queue submissions were rejected");
        }

        if (processed != orders.size())
        {
            throw std::runtime_error(
                "Not all submitted commands were processed");
        }
    }

    // Includes queue destruction and any node reclamation.
    lifecycleEnd = Clock::now();

    return {
        std::chrono::duration<double, std::milli>(
            processingEnd - start).count(),

        std::chrono::duration<double, std::milli>(
            lifecycleEnd - start).count()
    };
}

double percentile(const std::vector<double>& values, double p)
{
    const std::size_t index =
        static_cast<std::size_t>(p * (values.size() - 1));

    return values[index];
}

void printSummary(
    const char* name,
    const std::vector<double>& processing,
    const std::vector<double>& lifecycle,
    long long totalOrders)
{
    const int runs = static_cast<int>(processing.size());

    const double totalTime =
        std::accumulate(processing.begin(), processing.end(), 0.0);

    const double mean = totalTime / runs;

    double squaredDifferenceSum = 0.0;

    for (double timing : processing)
    {
        const double difference = timing - mean;
        squaredDifferenceSum += difference * difference;
    }

    const double standardDeviation =
        std::sqrt(squaredDifferenceSum / (runs - 1));

    const double cv = standardDeviation / mean;

    std::vector<double> sorted = processing;
    std::sort(sorted.begin(), sorted.end());

    double median;

    if (runs % 2 == 0)
    {
        median = (
            sorted[runs / 2 - 1] +
            sorted[runs / 2]) / 2.0;
    }
    else
    {
        median = sorted[runs / 2];
    }

    const double lifecycleTotal =
        std::accumulate(lifecycle.begin(), lifecycle.end(), 0.0);

    const double lifecycleMean = lifecycleTotal / runs;

    std::cout << "\n========== " << name << " ==========\n";
    std::cout << "Mean processing time: " << mean << " ms\n";
    std::cout << "Median: " << median << " ms\n";
    std::cout << "P50: " << percentile(sorted, 0.50) << " ms\n";
    std::cout << "P99: " << percentile(sorted, 0.99) << " ms\n";
    std::cout << "P99.9: " << percentile(sorted, 0.999) << " ms\n";
    std::cout << "Min: " << sorted.front() << " ms\n";
    std::cout << "Max: " << sorted.back() << " ms\n";
    std::cout << "Standard deviation: "
              << standardDeviation << " ms\n";
    std::cout << "CV: " << cv * 100.0 << "%\n";

    std::cout << "Processing throughput: "
              << totalOrders / (totalTime / 1000.0)
              << " orders/s\n";

    std::cout << "Mean full lifecycle time: "
              << lifecycleMean << " ms\n";

    std::cout << "Full lifecycle throughput: "
              << totalOrders / (lifecycleTotal / 1000.0)
              << " orders/s\n";
}

int main()
{
    constexpr int NUM_ORDERS = 50000;
    constexpr int NUM_PRODUCERS = 3;
    constexpr int WARMUP_RUNS = 3;
    constexpr int MEASURED_RUNS = 1000;

    // Identical controlled workload for both queues.
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> quantityDist(1, 100);

    std::vector<Order> orders;
    orders.reserve(NUM_ORDERS);

    for (int i = 0; i < NUM_ORDERS; ++i)
    {
        orders.push_back({
            i + 1,
            true,       // All BUY orders
            100.00,     // Same price
            quantityDist(rng)
        });
    }

    using MutexQueue = MutexPollingMPSCQueue<OrderCommand>;
    using LinkedQueue = LockFreeMSQueue<OrderCommand>;

    // Warm up both implementations.
    for (int i = 0; i < WARMUP_RUNS; ++i)
    {
        runOnce<MutexQueue>(orders, NUM_PRODUCERS);
        runOnce<LinkedQueue>(orders, NUM_PRODUCERS);
    }

    std::vector<double> mutexProcessing;
    std::vector<double> mutexLifecycle;
    std::vector<double> linkedProcessing;
    std::vector<double> linkedLifecycle;

    mutexProcessing.reserve(MEASURED_RUNS);
    mutexLifecycle.reserve(MEASURED_RUNS);
    linkedProcessing.reserve(MEASURED_RUNS);
    linkedLifecycle.reserve(MEASURED_RUNS);

    // Alternate which implementation runs first to reduce ordering bias.
    for (int i = 0; i < MEASURED_RUNS; ++i)
    {
        if (i % 2 == 0)
        {
            auto linked =
                runOnce<LinkedQueue>(orders, NUM_PRODUCERS);
            auto mutex =
                runOnce<MutexQueue>(orders, NUM_PRODUCERS);

            linkedProcessing.push_back(linked.processingMs);
            linkedLifecycle.push_back(linked.lifecycleMs);
            mutexProcessing.push_back(mutex.processingMs);
            mutexLifecycle.push_back(mutex.lifecycleMs);
        }
        else
        {
            auto mutex =
                runOnce<MutexQueue>(orders, NUM_PRODUCERS);
            auto linked =
                runOnce<LinkedQueue>(orders, NUM_PRODUCERS);

            mutexProcessing.push_back(mutex.processingMs);
            mutexLifecycle.push_back(mutex.lifecycleMs);
            linkedProcessing.push_back(linked.processingMs);
            linkedLifecycle.push_back(linked.lifecycleMs);
        }
    }

    const long long totalOrders =
        static_cast<long long>(NUM_ORDERS) * MEASURED_RUNS;

    std::cout << "Producers: " << NUM_PRODUCERS << '\n';
    std::cout << "Orders per run: " << NUM_ORDERS << '\n';
    std::cout << "Measured runs per implementation: "
              << MEASURED_RUNS << '\n';
    std::cout << "Total orders per implementation: "
              << totalOrders << '\n';

    printSummary(
        "Mutex polling MPSC queue",
        mutexProcessing,
        mutexLifecycle,
        totalOrders);

    printSummary(
        "Linked MS-style MPSC queue",
        linkedProcessing,
        linkedLifecycle,
        totalOrders);

    return 0;
}
