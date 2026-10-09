#pragma once

#include "Command.h"

#include <optional>
#include <condition_variable>
#include <mutex>
#include <queue>

class CommandQueue
{
public:
    bool push(OrderCommand command);

    std::optional<OrderCommand> waitAndPop();

    void close();

private:
    bool closed_ = false;
    std::queue<OrderCommand> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};