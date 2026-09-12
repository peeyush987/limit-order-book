#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include "Order.h"

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

    // Highest bid first
    std::map<double, std::list<Order>, std::greater<double>> bids;

    // Lowest ask first
    std::map<double, std::list<Order>> asks;

    // Order ID -> location of actual order
    std::unordered_map<int, OrderLocation> orderMap;

public:
    void addOrder(Order order);

    void matchBuyOrder(Order& order);

    void matchSellOrder(Order& order);

    void cancelOrder(int orderId);

    void modifyOrder(int orderId, int newQuantity, double newPrice);

    void printBook();
};

#endif