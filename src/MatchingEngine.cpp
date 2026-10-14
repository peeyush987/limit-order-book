#include <MatchingEngine.h>
#include <utility>

MatchingEngine::MatchingEngine()
    : orderBook_(0)
{}


void MatchingEngine::start()
{

    matchingThread_ =
        std::thread(&MatchingEngine::run, this);
}

bool MatchingEngine::submit(OrderCommand command)
{
    return commandQueue_.push(std::move(command));
}

void MatchingEngine::run()
{
    while (true)
    {
        auto nextCommand = commandQueue_.waitAndPop();

        if (!nextCommand)
        {
            break;
        }

        std::visit([this](auto&& command)
        {
            using CommandType =
                std::decay_t<decltype(command)>;

            if constexpr (
                std::is_same_v<CommandType, AddOrderCommand>)
            {
                orderBook_.addOrder(command.order);
            }
            else if constexpr (
                std::is_same_v<CommandType, CancelOrderCommand>)
            {
                orderBook_.cancelOrder(command.orderId);
            }
            else if constexpr (
                std::is_same_v<CommandType, ModifyOrderCommand>)
            {
                orderBook_.modifyOrder(
                    command.orderId,
                    command.newQuantity,
                    command.newPrice
                );
            }
            else if constexpr (
                std::is_same_v<CommandType, QueryHasOrderCommand>)
            {
                command.response.set_value(
                orderBook_.hasOrder(command.orderId)
                );
            }
        }, *nextCommand);
    }
}

void MatchingEngine::stop()
{
    commandQueue_.close();

    if (matchingThread_.joinable())
    {
        matchingThread_.join();
    }
}

MatchingEngine::~MatchingEngine()
{
    stop();
}