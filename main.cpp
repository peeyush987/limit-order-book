#include "OrderBook.h"
#include <iostream>

int main()
{
    OrderBook ob;

    // BUY FULLY FILLS SELL
    ob.addOrder({1, false, 100.0, 5});
    ob.addOrder({2, true, 105.0, 5});

    // BUY PARTIALLY FILLS SELL
    ob.addOrder({3, false, 105.0, 20});
    ob.addOrder({4, true, 108.0, 15});

    // BUY CONSUMES MULTIPLE SELLS
    ob.addOrder({5, false, 98.0, 5});
    ob.addOrder({6, true, 106.0, 100});

    // SELL FULLY FILLS BUY
    ob.addOrder({7, true, 100.0, 12});
    ob.addOrder({8, false, 99.0, 12});

    // SELL CONSUMES MULTIPLE BUYS
    ob.addOrder({9, true, 101.0, 10});
    ob.addOrder({10, true, 100.0, 5});
    ob.addOrder({11, false, 99.0, 15});

    // Cancellation test
    ob.cancelOrder(6);

    // Modification test
    ob.modifyOrder(9, 7, 102.0);

    ob.printBook();

    return 0;
}