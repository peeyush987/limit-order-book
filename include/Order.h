#ifndef ORDER_H
#define ORDER_H

struct Order {
    int id;
    bool isBuy;
    double price;
    int quantity;
};

#endif // ORDER_H