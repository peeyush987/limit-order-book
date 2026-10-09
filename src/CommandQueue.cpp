#include <CommandQueue.h>
#include <utility>

void CommandQueue::close()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
    }
    cv_.notify_all();
}


bool CommandQueue::push(OrderCommand command)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_)
        {
            return false;
        }
        queue_.push(std::move(command));
    }
    cv_.notify_one();
    return true;
}


std::optional<OrderCommand> CommandQueue::waitAndPop()
{
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return closed_ || !queue_.empty(); });
    if(queue_.empty())
    {
        return std::nullopt;
    }
    OrderCommand command = std::move(queue_.front());
    queue_.pop();
    return command;
}