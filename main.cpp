#include <iostream>

struct Order{
    int id;
    bool isBuy;
    double price;
    int quantity;
};

int main() {
    Order order1 = {1, true, 100.00, 10};
    Order order2 = {2, false, 102.00, 5};
    Order order3 = {3, true, 105.00, 20};


    std::cout << "Order 1"
                << " ID: " << order1.id
                << ", Type: " << (order1.isBuy ? "Buy" : "Sell")
                << ", Price: " << order1.price
                << ", Quantity: " << order1.quantity << std::endl;
    std::cout << "Order 2"
                << " ID: " << order2.id
                << ", Type: " << (order2.isBuy ? "Buy" : "Sell")
                << ", Price: " << order2.price
                << ", Quantity: " << order2.quantity << std::endl;
    std::cout << "Order 3"
                << " ID: " << order3.id
                << ", Type: " << (order3.isBuy ? "Buy" : "Sell")
                << ", Price: " << order3.price
                << ", Quantity: " << order3.quantity << std::endl;

                return 0;
}