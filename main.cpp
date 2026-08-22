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
    void addOrder(const Order& order){
        if(order.isBuy){
            bids[order.price].push(order);
        }else{
            asks[order.price].push(order);
        }
    }
    void printBook() {
        std::cout << "-- ASKS (sell) --\n";
        for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
            std::cout << it->first << " : " << it->second.size() << " order(s)\n";
        }
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
private:
    std::map<double, std::queue<Order>, std::greater<double>> bids; // highest price first
    std::map<double, std::queue<Order>> asks;                       // lowest price first
};


int main() {
    Order order1 = {1, true, 100.00, 10};
    Order order2 = {2, false, 102.00, 5};
    Order order3 = {3, true, 105.00, 20};
    Order order4 = {4, true, 108.00, 15};
    Order order5 = {5, false, 98.00, 8};
    Order order6 = {6, true, 100.00, 7};
    Order order7 = {7, true, 100.00, 12};

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
    ob.addOrder(order3);
    ob.addOrder(order4);
    ob.addOrder(order5);
    ob.addOrder(order6);
    ob.addOrder(order7);

    ob.printBook();

    return 0;
}