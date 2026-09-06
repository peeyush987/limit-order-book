#include <iostream>
#include <map>
#include <list>
#include <unordered_map>
#include <algorithm>

struct Order {
    int id;
    bool isBuy;
    double price;
    int quantity;
};

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

    // Order ID -> location of actual order in the book
    std::unordered_map<int, OrderLocation> orderMap;

public:

    // Work on a copy because matching modifies the remaining quantity.
    void addOrder(Order order) {

        if (order.isBuy) {

            matchBuyOrder(order);

            // If some quantity remains, the order becomes a resting bid.
            if (order.quantity > 0) {

                auto& orders = bids[order.price];

                // Insert at the back to preserve time priority.
                auto it = orders.insert(orders.end(), order);

                // Store iterator to the actual order in the list.
                orderMap[order.id] = {true, order.price, it};
            }

        } else {

            matchSellOrder(order);

            // If some quantity remains, the order becomes a resting ask.
            if (order.quantity > 0) {

                auto& orders = asks[order.price];

                // Insert at the back to preserve time priority.
                auto it = orders.insert(orders.end(), order);

                // Store iterator to the actual order in the list.
                orderMap[order.id] = {false, order.price, it};
            }
        }
    }

    void matchBuyOrder(Order& order) {

        while (!asks.empty() && order.quantity > 0) {

            // Best ask = lowest ask price
            auto mapIt = asks.begin();

            double bestAsk = mapIt->first;
            std::list<Order>& restingOrders = mapIt->second;

            // No more matches possible.
            if (order.price < bestAsk) {
                break;
            }

            // First order at this price = earliest order.
            auto orderIt = restingOrders.begin();
            Order& restingOrder = *orderIt;

            int tradeQuantity =
                std::min(order.quantity, restingOrder.quantity);

            // Reduce both orders.
            order.quantity -= tradeQuantity;
            restingOrder.quantity -= tradeQuantity;

            std::cout << "Trade executed: Buy Order ID "
                      << order.id
                      << " matched with Sell Order ID "
                      << restingOrder.id
                      << " for quantity "
                      << tradeQuantity
                      << " at price "
                      << bestAsk
                      << '\n';

            // Resting order completely filled.
            if (restingOrder.quantity == 0) {

                orderMap.erase(restingOrder.id);
                restingOrders.erase(orderIt);
            }

            // No orders remain at this price.
            if (restingOrders.empty()) {
                asks.erase(mapIt);
            }
        }
    }

    void matchSellOrder(Order& order) {

        while (!bids.empty() && order.quantity > 0) {

            // Best bid = highest bid price
            auto mapIt = bids.begin();

            double bestBid = mapIt->first;
            std::list<Order>& restingOrders = mapIt->second;

            // No more matches possible.
            if (order.price > bestBid) {
                break;
            }

            // First order at this price = earliest order.
            auto orderIt = restingOrders.begin();
            Order& restingOrder = *orderIt;

            int tradeQuantity =
                std::min(order.quantity, restingOrder.quantity);

            // Reduce both orders.
            order.quantity -= tradeQuantity;
            restingOrder.quantity -= tradeQuantity;

            std::cout << "Trade executed: Sell Order ID "
                      << order.id
                      << " matched with Buy Order ID "
                      << restingOrder.id
                      << " for quantity "
                      << tradeQuantity
                      << " at price "
                      << bestBid
                      << '\n';

            // Resting order completely filled.
            if (restingOrder.quantity == 0) {

                orderMap.erase(restingOrder.id);
                restingOrders.erase(orderIt);
            }

            // No orders remain at this price.
            if (restingOrders.empty()) {
                bids.erase(mapIt);
            }
        }
    }

    void cancelOrder(int orderId)
    {
        auto it = orderMap.find(orderId);

        if (it == orderMap.end()) {
            return;
        }

        OrderLocation location = it->second;

        if (location.isBuy)
        {
            auto& orders = bids.at(location.price);

            orders.erase(location.it);

            if (orders.empty()) {
                bids.erase(location.price);
            }
        }
        else
        {
            auto& orders = asks.at(location.price);

            orders.erase(location.it);

            if (orders.empty()) {
                asks.erase(location.price);
            }
        }

        orderMap.erase(it);
    }

    void printBook() {

        std::cout << "\n========== ORDER BOOK ==========\n";

        std::cout << "\n-- ASKS (SELL) --\n";

        for (const auto& [price, orders] : asks) {

            std::cout << price
                      << " : "
                      << orders.size()
                      << " order(s)\n";

            for (const auto& order : orders) {

                std::cout << "  ID: "
                          << order.id
                          << ", Quantity: "
                          << order.quantity
                          << '\n';
            }
        }

        std::cout << "\n-- BIDS (BUY) --\n";

        for (const auto& [price, orders] : bids) {

            std::cout << price
                      << " : "
                      << orders.size()
                      << " order(s)\n";

            for (const auto& order : orders) {

                std::cout << "  ID: "
                          << order.id
                          << ", Quantity: "
                          << order.quantity
                          << '\n';
            }
        }

        std::cout << "\n===============================\n";
    }
};


int main() {

    // BUY FULLY FILLS SELL
    Order order1 = {1, false, 100.00, 5};
    Order order2 = {2, true, 105.00, 5};

    // BUY PARTIALLY FILLS SELL
    Order order3 = {3, false, 105.00, 20};
    Order order4 = {4, true, 108.00, 15};

    // BUY CONSUMES MULTIPLE SELLS
    Order order5 = {5, false, 98.00, 5};
    Order order6 = {6, true, 106.00, 100};

    // SELL FULLY FILLS BUY
    Order order7 = {7, true, 100.00, 12};
    Order order8 = {8, false, 99.00, 12};

    // SELL CONSUMES MULTIPLE BUYS
    Order order9  = {9, true, 101.00, 10};
    Order order10 = {10, true, 100.00, 5};
    Order order11 = {11, false, 99.00, 15};

    OrderBook ob;

    ob.addOrder(order1);
    ob.addOrder(order2);

    ob.addOrder(order3);
    ob.addOrder(order4);

    ob.addOrder(order5);
    ob.addOrder(order6);

    ob.addOrder(order7);
    ob.cancelOrder(6);
    ob.addOrder(order8);

    ob.addOrder(order9);
    ob.addOrder(order10);
    ob.addOrder(order11);

    ob.printBook();

    return 0;
}