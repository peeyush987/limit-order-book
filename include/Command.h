#pragma once

#include "Order.h"
#include <future>
#include <variant>

struct AddOrderCommand
{
    Order order;
};

struct CancelOrderCommand
{
    int orderId;
};

struct ModifyOrderCommand
{
    int orderId;
    int newQuantity;
    double newPrice;
};

struct QueryHasOrderCommand
{
    int orderId;
    std::promise<bool> response;
};

using OrderCommand = std::variant<
    AddOrderCommand,
    CancelOrderCommand,
    ModifyOrderCommand,
    QueryHasOrderCommand
>;