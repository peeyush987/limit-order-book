#include <CommandQueue.h>
#include <utility>

void CommandQueue::push(OrderCommand command)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(command));
    }
    cv_.notify_one();
}


OrderCommand CommandQueue::waitAndPop()
{
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !queue_.empty(); });
    OrderCommand command = std::move(queue_.front());
    queue_.pop();
    return command;
}