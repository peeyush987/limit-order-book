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

    void modifyOrder(int orderId, int newQuantity, double newPrice){

        auto it = orderMap.find(orderId);

        if(it == orderMap.end())
        {
            return;
        }

        OrderLocation location = it->second;

        if(newQuantity < 0)
        {
            std::cout << "Error: Quantity cannot be negative. Order not modified." << std::endl;
            return;
        }

        if(newQuantity == 0)
        {
            cancelOrder(orderId);
            return;
        }
        
        if (newPrice <= 0) {
            std::cout << "Error: Price cannot be zero or negative. Order not modified." << std::endl;
            return;
        }

        if(newPrice != location.price )
        {
            cancelOrder(orderId);

            Order modifiedOrder = {orderId, location.isBuy, newPrice, newQuantity};

            addOrder(modifiedOrder);
        }
        else
        {
            location.it->quantity = newQuantity;
        }
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

    // =========================================================
    // 1. BUY FULLY FILLS SELL
    // =========================================================
    {
        std::cout << "\n===== TEST 1: BUY FULLY FILLS SELL =====\n";

        OrderBook ob;

        Order sell = {1, false, 100.0, 5};
        Order buy  = {2, true, 105.0, 5};

        ob.addOrder(sell);
        ob.addOrder(buy);

        ob.printBook();
    }


    // =========================================================
    // 2. BUY PARTIALLY FILLS SELL
    // =========================================================
    {
        std::cout << "\n===== TEST 2: BUY PARTIALLY FILLS SELL =====\n";

        OrderBook ob;

        Order sell = {1, false, 100.0, 10};
        Order buy  = {2, true, 105.0, 6};

        ob.addOrder(sell);
        ob.addOrder(buy);

        ob.printBook();
    }


    // =========================================================
    // 3. SELL PARTIALLY FILLS BUY
    // =========================================================
    {
        std::cout << "\n===== TEST 3: SELL PARTIALLY FILLS BUY =====\n";

        OrderBook ob;

        Order buy  = {1, true, 105.0, 10};
        Order sell = {2, false, 100.0, 6};

        ob.addOrder(buy);
        ob.addOrder(sell);

        ob.printBook();
    }


    // =========================================================
    // 4. BUY CONSUMES MULTIPLE SELL PRICE LEVELS
    // =========================================================
    {
        std::cout << "\n===== TEST 4: BUY CONSUMES MULTIPLE SELLS =====\n";

        OrderBook ob;

        Order sell1 = {1, false, 98.0, 5};
        Order sell2 = {2, false, 100.0, 3};
        Order sell3 = {3, false, 103.0, 7};

        Order buy = {4, true, 105.0, 10};

        ob.addOrder(sell1);
        ob.addOrder(sell2);
        ob.addOrder(sell3);

        ob.addOrder(buy);

        ob.printBook();
    }


    // =========================================================
    // 5. SELL CONSUMES MULTIPLE BUY PRICE LEVELS
    // =========================================================
    {
        std::cout << "\n===== TEST 5: SELL CONSUMES MULTIPLE BUYS =====\n";

        OrderBook ob;

        Order buy1 = {1, true, 105.0, 5};
        Order buy2 = {2, true, 103.0, 3};
        Order buy3 = {3, true, 100.0, 7};

        Order sell = {4, false, 98.0, 10};

        ob.addOrder(buy1);
        ob.addOrder(buy2);
        ob.addOrder(buy3);

        ob.addOrder(sell);

        ob.printBook();
    }


    // =========================================================
    // 6. NON-CROSSING BUY RESTS IN BOOK
    // =========================================================
    {
        std::cout << "\n===== TEST 6: NON-CROSSING BUY =====\n";

        OrderBook ob;

        Order sell = {1, false, 105.0, 5};
        Order buy  = {2, true, 100.0, 10};

        ob.addOrder(sell);
        ob.addOrder(buy);

        ob.printBook();
    }


    // =========================================================
    // 7. NON-CROSSING SELL RESTS IN BOOK
    // =========================================================
    {
        std::cout << "\n===== TEST 7: NON-CROSSING SELL =====\n";

        OrderBook ob;

        Order buy  = {1, true, 100.0, 5};
        Order sell = {2, false, 105.0, 10};

        ob.addOrder(buy);
        ob.addOrder(sell);

        ob.printBook();
    }


    // =========================================================
    // 8. EXACT PRICE MATCH
    // =========================================================
    {
        std::cout << "\n===== TEST 8: EXACT PRICE MATCH =====\n";

        OrderBook ob;

        Order sell = {1, false, 100.0, 5};
        Order buy  = {2, true, 100.0, 5};

        ob.addOrder(sell);
        ob.addOrder(buy);

        ob.printBook();
    }


    // =========================================================
    // 9. FIFO AT SAME SELL PRICE
    // =========================================================
    {
        std::cout << "\n===== TEST 9: FIFO SELL ORDERS =====\n";

        OrderBook ob;

        Order sell1 = {1, false, 100.0, 5};
        Order sell2 = {2, false, 100.0, 7};
        Order sell3 = {3, false, 100.0, 4};

        Order buy = {4, true, 105.0, 8};

        ob.addOrder(sell1);
        ob.addOrder(sell2);
        ob.addOrder(sell3);

        ob.addOrder(buy);

        ob.printBook();
    }


    // =========================================================
    // 10. FIFO AT SAME BUY PRICE
    // =========================================================
    {
        std::cout << "\n===== TEST 10: FIFO BUY ORDERS =====\n";

        OrderBook ob;

        Order buy1 = {1, true, 100.0, 5};
        Order buy2 = {2, true, 100.0, 7};
        Order buy3 = {3, true, 100.0, 4};

        Order sell = {4, false, 95.0, 8};

        ob.addOrder(buy1);
        ob.addOrder(buy2);
        ob.addOrder(buy3);

        ob.addOrder(sell);

        ob.printBook();
    }


    // =========================================================
    // 11. CANCEL MIDDLE ORDER
    // =========================================================
    {
        std::cout << "\n===== TEST 11: CANCEL MIDDLE ORDER =====\n";

        OrderBook ob;

        Order buy1 = {1, true, 100.0, 5};
        Order buy2 = {2, true, 100.0, 5};
        Order buy3 = {3, true, 100.0, 5};

        ob.addOrder(buy1);
        ob.addOrder(buy2);
        ob.addOrder(buy3);

        ob.cancelOrder(2);

        ob.printBook();
    }


    // =========================================================
    // 12. CANCEL ONLY ORDER AT A PRICE
    // =========================================================
    {
        std::cout << "\n===== TEST 12: CANCEL ONLY ORDER =====\n";

        OrderBook ob;

        Order buy = {1, true, 100.0, 5};

        ob.addOrder(buy);
        ob.cancelOrder(1);

        ob.printBook();
    }


    // =========================================================
    // 13. CANCEL NON-EXISTENT ORDER
    // =========================================================
    {
        std::cout << "\n===== TEST 13: CANCEL NON-EXISTENT ORDER =====\n";

        OrderBook ob;

        ob.cancelOrder(999);

        ob.printBook();
    }


    // =========================================================
    // 14. QUANTITY-ONLY MODIFICATION
    //     Should preserve FIFO position
    // =========================================================
    {
        std::cout << "\n===== TEST 14: QUANTITY MODIFICATION =====\n";

        OrderBook ob;

        Order buy1 = {1, true, 100.0, 10};
        Order buy2 = {2, true, 100.0, 10};
        Order buy3 = {3, true, 100.0, 10};

        ob.addOrder(buy1);
        ob.addOrder(buy2);
        ob.addOrder(buy3);

        ob.modifyOrder(2, 5, 100.0);

        ob.printBook();
    }


    // =========================================================
    // 15. PRICE MODIFICATION
    // =========================================================
    {
        std::cout << "\n===== TEST 15: PRICE MODIFICATION =====\n";

        OrderBook ob;

        Order buy = {1, true, 100.0, 10};

        ob.addOrder(buy);

        ob.modifyOrder(1, 10, 105.0);

        ob.printBook();
    }


    // =========================================================
    // 16. PRICE MODIFICATION GETS NEW FIFO POSITION
    // =========================================================
    {
        std::cout << "\n===== TEST 16: PRICE MODIFICATION + FIFO =====\n";

        OrderBook ob;

        Order buy1 = {1, true, 100.0, 5};
        Order buy2 = {2, true, 101.0, 5};

        ob.addOrder(buy1);
        ob.addOrder(buy2);

        // Order 1 moves from 100 -> 101
        ob.modifyOrder(1, 5, 101.0);

        // At 101, Order 2 should be ahead of Order 1
        ob.printBook();
    }


    // =========================================================
    // 17. PRICE MODIFICATION IMMEDIATELY MATCHES
    // =========================================================
    {
        std::cout << "\n===== TEST 17: MODIFICATION TRIGGERS MATCH =====\n";

        OrderBook ob;

        Order sell = {1, false, 102.0, 5};
        Order buy  = {2, true, 100.0, 5};

        ob.addOrder(sell);
        ob.addOrder(buy);

        // BUY changes from 100 -> 105 and should now match
        ob.modifyOrder(2, 5, 105.0);

        ob.printBook();
    }


    // =========================================================
    // 18. MODIFY QUANTITY TO ZERO
    // =========================================================
    {
        std::cout << "\n===== TEST 18: MODIFY QUANTITY TO ZERO =====\n";

        OrderBook ob;

        Order buy = {1, true, 100.0, 5};

        ob.addOrder(buy);

        ob.modifyOrder(1, 0, 100.0);

        ob.printBook();
    }


    // =========================================================
    // 19. MODIFY NON-EXISTENT ORDER
    // =========================================================
    {
        std::cout << "\n===== TEST 19: MODIFY NON-EXISTENT ORDER =====\n";

        OrderBook ob;

        ob.modifyOrder(999, 10, 100.0);

        ob.printBook();
    }


    // =========================================================
    // 20. MODIFY PRICE + PARTIAL MATCH
    // =========================================================
    {
        std::cout << "\n===== TEST 20: MODIFICATION + PARTIAL MATCH =====\n";

        OrderBook ob;

        Order sell = {1, false, 102.0, 10};
        Order buy  = {2, true, 100.0, 5};

        ob.addOrder(sell);
        ob.addOrder(buy);

        // Modified BUY becomes marketable
        ob.modifyOrder(2, 7, 105.0);

        ob.printBook();
    }

    return 0;
}