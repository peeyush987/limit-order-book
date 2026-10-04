#include "OrderBook.h"
#include <iostream>

int main()
{
    OrderBook book;

    book.addOrder({1, false, 100.0, 10});
    book.addOrder({2, true, 105.0, 6});

    book.printBook();

    return 0;
}