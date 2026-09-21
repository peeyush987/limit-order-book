#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include "PoolAllocator.h"
#include "Order.h"
#include "Trade.h"

#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

using OrderList = std::list<Order, PoolAllocator<Order>>;

class OrderBook {
private:
    struct OrderLocation {
        bool isBuy;
        double price;
        OrderList::iterator it;
    };

    std::vector<Trade> tradeHistory; // Global trade history

    // Highest bid first
    std::map<double, OrderList, std::greater<double>> bids;

    // Lowest ask first
    std::map<double, OrderList> asks;

    // Order ID -> location of actual order
    std::unordered_map<int, OrderLocation> orderMap;

public:
    void addOrder(Order order);

    std::vector<Trade> matchBuyOrder(Order& order);

    std::vector<Trade> matchSellOrder(Order& order);

    void cancelOrder(int orderId);

    void modifyOrder(int orderId, int newQuantity, double newPrice);

    void printBook();

    bool hasOrder(int orderId) const;

    int getOrderQuantity(int orderId) const;

    bool hasPriceLevel(bool isBuy, double price) const;

    const std::vector<Trade>& getTradeHistory() const
    {
        return tradeHistory;
    }
};

#endif