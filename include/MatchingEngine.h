#pragma once

#include "CommandQueue.h"
#include "OrderBook.h"

#include <thread>

class MatchingEngine
{
public:
    MatchingEngine();

    void start();
    void stop();

    bool submit(OrderCommand command);

    ~MatchingEngine();

private:
    void run();

    CommandQueue commandQueue_;
    OrderBook orderBook_;
    std::thread matchingThread_;
};