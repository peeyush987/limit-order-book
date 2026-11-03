#include "OrderBook.h"

#include <cstddef>
#include <iostream>

static void showNewTrades(const OrderBook& book, std::size_t& seen)
{
    const auto& trades = book.getTradeHistory();

    if (trades.size() == seen)
    {
        std::cout << "  (no trades)\n";
    }

    for (; seen < trades.size(); ++seen)
    {
        const Trade& t = trades[seen];
        std::cout << "  TRADE: buy #" << t.buyOrderId
                  << " / sell #" << t.sellOrderId
                  << "  " << t.quantity << " @ " << t.price << "\n";
    }
}

int main()
{
    OrderBook book;
    std::size_t seen = 0;

    std::cout << "=== Step 1: three resting SELLs "
                 "(#1 10@101, #2 5@101 later, #3 8@102) ===\n";
    book.addOrder({1, false, 101.0, 10});
    book.addOrder({2, false, 101.0, 5});
    book.addOrder({3, false, 102.0, 8});
    showNewTrades(book, seen);
    book.printBook();

    std::cout << "\n=== Step 2: BUY #4 12@101.50 "
                 "(crosses #1 fully, then #2 partially, FIFO) ===\n";
    book.addOrder({4, true, 101.5, 12});
    showNewTrades(book, seen);
    book.printBook();

    std::cout << "\n=== Step 3: BUY #5 20@103 "
                 "(sweeps two price levels, remainder rests) ===\n";
    book.addOrder({5, true, 103.0, 20});
    showNewTrades(book, seen);
    book.printBook();

    std::cout << "\n=== Step 4: SELL #6 4@103 "
                 "(hits the resting bid at an exact price) ===\n";
    book.addOrder({6, false, 103.0, 4});
    showNewTrades(book, seen);
    book.printBook();

    std::cout << "\n=== Step 5: cancel #5 ===\n";
    book.cancelOrder(5);
    showNewTrades(book, seen);
    book.printBook();

    return 0;
}