#include "MatchingEngine.h"

#include <future>
#include <stdexcept>
#include <type_traits>
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
            else if constexpr (
                std::is_same_v<CommandType, QueryOrderQuantityCommand>)
            {
                command.response.set_value(
                orderBook_.getOrderQuantity(command.orderId)
                );
            }
        }, *nextCommand);
    }
}

bool MatchingEngine::hasOrder(int orderId)
{
    QueryHasOrderCommand query{
        orderId,
        std::promise<bool>{}
    };

    std::future<bool> result = query.response.get_future();

    if (!submit(std::move(query)))
    {
        throw std::runtime_error(
            "Cannot query OrderBook: command queue is closed"
        );
    }

    return result.get();
}

void MatchingEngine::stop()
{
    commandQueue_.close();

    if (matchingThread_.joinable())
    {
        matchingThread_.join();
    }
}

int MatchingEngine::getOrderQuantity(int orderId)
{
    QueryOrderQuantityCommand query{
        orderId,
        std::promise<int>{}
    };

    std::future<int> result = query.response.get_future();

    if (!submit(std::move(query)))
    {
        throw std::runtime_error(
            "Cannot query OrderBook: command queue is closed"
        );
    }

    return result.get();
}

MatchingEngine::~MatchingEngine()
{
    stop();
}