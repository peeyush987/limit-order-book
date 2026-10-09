#pragma once

#include "CommandQueue.h"
#include "OrderBook.h"

#include <atomic>
#include <thread>

class MatchingEngine
{
public:
    MatchingEngine();

    void start();
    void stop();

    void submit(OrderCommand command);

private:
    void run();

    CommandQueue commandQueue_;
    OrderBook orderBook_;
    std::thread matchingThread_;
    std::atomic<bool> running_;
};