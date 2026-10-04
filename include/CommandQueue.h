#pragma once

#include "Command.h"

#include <condition_variable>
#include <mutex>
#include <queue>

class CommandQueue
{
public:
    void push(OrderCommand command);

    OrderCommand waitAndPop();

private:
    std::queue<OrderCommand> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};