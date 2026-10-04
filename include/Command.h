#pragma once

#include "Order.h"

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

using OrderCommand = std::variant<
    AddOrderCommand,
    CancelOrderCommand,
    ModifyOrderCommand
>;