#include <map>
#include <queue>
#include <iostream>

struct Order{
    int id;
    bool isBuy;
    double price;
    int quantity;
};

class OrderBook {
public:
    // Work on a copy because matching reduces the remaining quantity.
    void addOrder( Order order){
        if(order.isBuy){
            matchBuyOrder(order);

            if(order.quantity > 0)
            {
                bids[order.price].push(order);
            }
        }
        else{

            matchSellOrder(order);

            if(order.quantity > 0){
                asks[order.price].push(order);
            }

        }
    }
    void printBook() {

        std::cout << "-- ASKS (sell) --\n";

        for (auto& [price, orders] : asks) {

            std::cout << price << " : "
                    << orders.size() << " order(s)\n";

            auto temp = orders;

            while (!temp.empty()) {

                const auto& order = temp.front();

                std::cout << "  ID: " << order.id
                        << ", Quantity: " << order.quantity << "\n";

                temp.pop();
            }
        }

        std::cout << "-- BIDS (buy) --\n";

        for (auto& [price, orders] : bids) {

            std::cout << price << " : "
                    << orders.size() << " order(s)\n";

            auto temp = orders;

            while (!temp.empty()) {

                const auto& order = temp.front();

                std::cout << "  ID: " << order.id
                        << ", Quantity: " << order.quantity << "\n";

                temp.pop();
            }
        }

    }
    void matchSellOrder(Order& order)
    {
        while( !bids.empty() && order.quantity > 0)
        {
            auto it = bids.begin();
            std::queue<Order>& restingqueue = it->second;
            double bestbid = it->first;

            if(order.price > bestbid)
            {
                break;
            }
            Order& restingorder = restingqueue.front();
            int tradequantity = std::min(order.quantity, restingorder.quantity);
            order.quantity -= tradequantity;
            restingorder.quantity -= tradequantity;
            std::cout << "Trade executed: Sell Order ID " << order.id
                      << " matched with Buy Order ID " << restingorder.id
                      << " for quantity " << tradequantity
                      << " at price " << bestbid << std::endl;
            if(restingorder.quantity == 0)
            {
                restingqueue.pop();
            }
            if(restingqueue.empty())
            {
                bids.erase(it);
            }
        }
    }
    void matchBuyOrder(Order& order)
    {
        while (!asks.empty() && order.quantity > 0)
        {
            // 1. Get best ask
            auto it = asks.begin();
            std::queue<Order>& restingqueue = it->second;
            double bestask = it->first;
            // 2. Check whether prices cross
            if(order.price < bestask)
            {
                break; // No match possible
            }
            // 3. Get first order in that price queue
            Order& restingorder = restingqueue.front();
            // 4. Calculate trade quantity
            int tradequantity = std::min(order.quantity, restingorder.quantity);
            // 5. Reduce both quantities
            order.quantity -= tradequantity;
            restingorder.quantity -= tradequantity;
            std::cout << "Trade executed: Buy Order ID " << order.id
                      << " matched with Sell Order ID " << restingorder.id
                      << " for quantity " << tradequantity
                      << " at price " << bestask << std::endl;
            // 6. Remove filled order
            if (restingorder.quantity == 0)
            {
                restingqueue.pop();
            }
            // 7. Remove empty price level
            if(restingqueue.empty())
            {
                asks.erase(it);
            }
        }
    }
private:
    std::map<double, std::queue<Order>, std::greater<double>> bids; // highest price first
    std::map<double, std::queue<Order>> asks;                       // lowest price first
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
    Order order6 = {6, true, 106.00, 10};
    // SELL FULLY FILLS BUY
    Order order7 = {7, true, 100.00, 12};
    Order order8 = {8, false, 99.00, 12};
    // SELL CONSUMES MULTIPLE BUYS
    Order order9 = {9, true, 101.00, 10};
    Order order10 = {10, true, 100.00, 5};
    Order order11 = {11, false, 99.00, 15};
    



    // std::cout << "Order 1"
    //             << " ID: " << order1.id
    //             << ", Type: " << (order1.isBuy ? "Buy" : "Sell")
    //             << ", Price: " << order1.price
    //             << ", Quantity: " << order1.quantity << std::endl;
    // std::cout << "Order 2"
    //             << " ID: " << order2.id
    //             << ", Type: " << (order2.isBuy ? "Buy" : "Sell")
    //             << ", Price: " << order2.price
    //             << ", Quantity: " << order2.quantity << std::endl;
    // std::cout << "Order 3"
    //             << " ID: " << order3.id
    //             << ", Type: " << (order3.isBuy ? "Buy" : "Sell")
    //             << ", Price: " << order3.price
    //             << ", Quantity: " << order3.quantity << std::endl;

    OrderBook ob;
    ob.addOrder(order1);
    ob.addOrder(order2);
    // Match the buy order with existing sell orders
    ob.addOrder(order3);
    ob.addOrder(order4);
    ob.addOrder(order5);
    ob.addOrder(order6);
    ob.addOrder(order7);
    ob.addOrder(order8);
    ob.addOrder(order9);
    ob.addOrder(order10);
    ob.addOrder(order11);

    ob.printBook();

    return 0;
}


