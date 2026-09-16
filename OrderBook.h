#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include "Order.h"
#include "Trade.h"
#include <vector>

#include <list>
#include <map>
#include <unordered_map>

class OrderBook {
private:
    struct OrderLocation {
        bool isBuy;
        double price;
        std::list<Order>::iterator it;
    };

    std::vector<Trade> tradeHistory; // Global trade history

    // Highest bid first
    std::map<double, std::list<Order>, std::greater<double>> bids;

    // Lowest ask first
    std::map<double, std::list<Order>> asks;

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