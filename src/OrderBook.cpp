#include "OrderBook.h"

#include <algorithm>
#include <iostream>

OrderBook::OrderBook(std::size_t expectedOrders)
{
    if (expectedOrders > 0)
    {
        orderMap.reserve(expectedOrders);
        tradeHistory.reserve(expectedOrders);
    }
}



void OrderBook::addOrderUnlocked(Order order) {
    if (order.isBuy)
    {
        matchBuyOrderUnlocked(order);

        // tradeHistory.insert(tradeHistory.end(), trades.begin(), trades.end());

        if (order.quantity > 0)
        {
            auto& orders = bids[order.price];

            auto it = orders.insert(orders.end(), order);

            orderMap[order.id] = {true, order.price, it};
        }
    }
    else
    {
        matchSellOrderUnlocked(order);

        // tradeHistory.insert(tradeHistory.end(), trades.begin(), trades.end());

        if (order.quantity > 0)
        {
            auto& orders = asks[order.price];

            auto it = orders.insert(orders.end(), order);

            orderMap[order.id] = {false, order.price, it};
        }
    }
}

void OrderBook::addOrder(Order order)
{
    std::lock_guard<std::mutex> lock(mutex_);
    addOrderUnlocked(order);

}

bool OrderBook::hasOrder(int orderId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return orderMap.find(orderId) != orderMap.end();
}

bool OrderBook::hasPriceLevel(bool isBuy, double price) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (isBuy)
    {
        return bids.find(price) != bids.end();
    }
    else
    {
        return asks.find(price) != asks.end();
    }
}

int OrderBook::getOrderQuantity(int orderId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = orderMap.find(orderId);

    if (it == orderMap.end())
    {
        return -1;
    }

    return it->second.it->quantity;
}

void OrderBook::matchBuyOrderUnlocked(Order& order)
{
    // std::vector<Trade> trades;

    while (!asks.empty() && order.quantity > 0)
    {
        auto mapIt = asks.begin();

        double bestAsk = mapIt->first;
        auto& restingOrders = mapIt->second;

        if (order.price < bestAsk)
        {
            break;
        }

        auto orderIt = restingOrders.begin();
        Order& restingOrder = *orderIt;

        int tradeQuantity =
            std::min(order.quantity, restingOrder.quantity);

        order.quantity -= tradeQuantity;
        restingOrder.quantity -= tradeQuantity;

        tradeHistory.push_back({order.id, restingOrder.id, bestAsk, tradeQuantity});

        if (restingOrder.quantity == 0)
        {
            orderMap.erase(restingOrder.id);
            restingOrders.erase(orderIt);
        }

        if (restingOrders.empty())
        {
            asks.erase(mapIt);
        }
    }
    // return trades;
}


void OrderBook::matchSellOrderUnlocked(Order& order)
{
    // std::vector<Trade> trades;

    while (!bids.empty() && order.quantity > 0)
    {
        auto mapIt = bids.begin();

        double bestBid = mapIt->first;
        auto& restingOrders = mapIt->second;

        if (order.price > bestBid)
        {
            break;
        }

        auto orderIt = restingOrders.begin();
        Order& restingOrder = *orderIt;

        int tradeQuantity =
            std::min(order.quantity, restingOrder.quantity);

        order.quantity -= tradeQuantity;
        restingOrder.quantity -= tradeQuantity;

        tradeHistory.push_back({restingOrder.id, order.id, bestBid, tradeQuantity});


        if (restingOrder.quantity == 0)
        {
            orderMap.erase(restingOrder.id);
            restingOrders.erase(orderIt);
        }

        if (restingOrders.empty())
        {
            bids.erase(mapIt);
        }
    }
    // return trades;
}

void OrderBook::cancelOrderUnlocked(int orderId) {
    auto it = orderMap.find(orderId);

    if (it == orderMap.end())
    {
        return;
    }

    OrderLocation location = it->second;

    if (location.isBuy)
    {
        auto& orders = bids.at(location.price);

        orders.erase(location.it);

        if (orders.empty())
        {
            bids.erase(location.price);
        }
    }
    else
    {
        auto& orders = asks.at(location.price);

        orders.erase(location.it);

        if (orders.empty())
        {
            asks.erase(location.price);
        }
    }

    orderMap.erase(it);
}


void OrderBook::cancelOrder(int orderId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    cancelOrderUnlocked(orderId);
}


void OrderBook::modifyOrder(int orderId, int newQuantity, double newPrice)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = orderMap.find(orderId);

    if (it == orderMap.end())
    {
        return;
    }

    OrderLocation location = it->second;

    if (newQuantity < 0)
    {
        std::cout << "Error: Quantity cannot be negative. "
                  << "Order not modified.\n";
        return;
    }

    if (newQuantity == 0)
    {
        cancelOrderUnlocked(orderId);
        return;
    }

    if (newPrice <= 0)
    {
        std::cout << "Error: Price cannot be zero or negative. "
                  << "Order not modified.\n";
        return;
    }

    if (newPrice != location.price)
    {
        cancelOrderUnlocked(orderId);

        Order modifiedOrder = {
            orderId,
            location.isBuy,
            newPrice,
            newQuantity
        };

        addOrderUnlocked(modifiedOrder);
    }
    else
    {
        location.it->quantity = newQuantity;
    }
}


void OrderBook::printBook()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "\n========== ORDER BOOK ==========\n";

    std::cout << "\n-- ASKS (SELL) --\n";

    for (const auto& [price, orders] : asks)
    {
        std::cout << price
                  << " : "
                  << orders.size()
                  << " order(s)\n";

        for (const auto& order : orders)
        {
            std::cout << "  ID: "
                      << order.id
                      << ", Quantity: "
                      << order.quantity
                      << '\n';
        }
    }

    std::cout << "\n-- BIDS (BUY) --\n";

    for (const auto& [price, orders] : bids)
    {
        std::cout << price
                  << " : "
                  << orders.size()
                  << " order(s)\n";

        for (const auto& order : orders)
        {
            std::cout << "  ID: "
                      << order.id
                      << ", Quantity: "
                      << order.quantity
                      << '\n';
        }
    }

    std::cout << "\n===============================\n";
}