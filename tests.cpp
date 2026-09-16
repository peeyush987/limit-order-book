#include "OrderBook.h"
#include <cassert>
#include <iostream>
#include <vector>

// Test helper: compares the complete trade history
// against the expected trades.
void assertTradesEqual(
    const std::vector<Trade>& actual,
    const std::vector<Trade>& expected)
{
    assert(actual.size() == expected.size());

    for (size_t i = 0; i < expected.size(); ++i)
    {
        assert(actual[i].buyOrderId == expected[i].buyOrderId);
        assert(actual[i].sellOrderId == expected[i].sellOrderId);
        assert(actual[i].price == expected[i].price);
        assert(actual[i].quantity == expected[i].quantity);
    }
}

void testBuyFullyFillsSell()
{
    OrderBook ob;

    ob.addOrder({1, false, 100.0, 5});
    ob.addOrder({2, true, 105.0, 5});

    std::vector<Trade> expected = {
        {2, 1, 100.0, 5}
    };

    assertTradesEqual(ob.getTradeHistory(), expected);

    assert(!ob.hasOrder(1));
    assert(!ob.hasOrder(2));
    assert(!ob.hasPriceLevel(false, 100.0));

    std::cout << "PASS: BUY fully fills SELL\n";
}

void testBuyPartiallyFillsSell()
{
    OrderBook ob;

    ob.addOrder({1, false, 100.0, 10});
    ob.addOrder({2, true, 105.0, 6});

    std::vector<Trade> expected = {
        {2, 1, 100.0, 6}
    };

    assertTradesEqual(ob.getTradeHistory(), expected);

    assert(ob.hasOrder(1));
    assert(ob.getOrderQuantity(1) == 4);
    assert(!ob.hasOrder(2));

    std::cout << "PASS: BUY partially fills SELL\n";
}

void testSellPartiallyFillsBuy()
{
    OrderBook ob;

    ob.addOrder({1, true, 105.0, 10});
    ob.addOrder({2, false, 100.0, 6});

    std::vector<Trade> expected = {
        {1, 2, 105.0, 6}
    };

    assertTradesEqual(ob.getTradeHistory(), expected);

    assert(ob.hasOrder(1));
    assert(ob.getOrderQuantity(1) == 4);
    assert(!ob.hasOrder(2));

    std::cout << "PASS: SELL partially fills BUY\n";
}

void testBuyConsumesMultipleSellLevels()
{
    OrderBook ob;

    ob.addOrder({1, false, 98.0, 5});
    ob.addOrder({2, false, 100.0, 3});
    ob.addOrder({3, false, 103.0, 7});

    ob.addOrder({4, true, 105.0, 10});

    std::vector<Trade> expected = {
        {4, 1, 98.0, 5},
        {4, 2, 100.0, 3},
        {4, 3, 103.0, 2}
    };

    assertTradesEqual(ob.getTradeHistory(), expected);

    assert(!ob.hasOrder(1));
    assert(!ob.hasOrder(2));
    assert(ob.hasOrder(3));
    assert(ob.getOrderQuantity(3) == 5);
    assert(!ob.hasOrder(4));

    std::cout << "PASS: BUY consumes multiple SELL levels\n";
}

void testSellConsumesMultipleBuyLevels()
{
    OrderBook ob;

    ob.addOrder({1, true, 105.0, 5});
    ob.addOrder({2, true, 103.0, 3});
    ob.addOrder({3, true, 100.0, 7});

    ob.addOrder({4, false, 98.0, 10});

    std::vector<Trade> expected = {
        {1, 4, 105.0, 5},
        {2, 4, 103.0, 3},
        {3, 4, 100.0, 2}
    };

    assertTradesEqual(ob.getTradeHistory(), expected);

    assert(!ob.hasOrder(1));
    assert(!ob.hasOrder(2));
    assert(ob.hasOrder(3));
    assert(ob.getOrderQuantity(3) == 5);
    assert(!ob.hasOrder(4));

    std::cout << "PASS: SELL consumes multiple BUY levels\n";
}

void testNonCrossingBuyRests()
{
    OrderBook ob;

    ob.addOrder({1, false, 105.0, 5});
    ob.addOrder({2, true, 100.0, 10});

    std::vector<Trade> expected = {};

    assertTradesEqual(ob.getTradeHistory(), expected);

    assert(ob.hasOrder(1));
    assert(ob.hasOrder(2));

    std::cout << "PASS: non-crossing BUY rests\n";
}

void testNonCrossingSellRests()
{
    OrderBook ob;

    ob.addOrder({1, true, 100.0, 5});
    ob.addOrder({2, false, 105.0, 10});

    std::vector<Trade> expected = {};

    assertTradesEqual(ob.getTradeHistory(), expected);

    assert(ob.hasOrder(1));
    assert(ob.hasOrder(2));

    std::cout << "PASS: non-crossing SELL rests\n";
}

void testExactPriceMatch()
{
    OrderBook ob;

    ob.addOrder({1, false, 100.0, 5});
    ob.addOrder({2, true, 100.0, 5});

    std::vector<Trade> expected = {
        {2, 1, 100.0, 5}
    };

    assertTradesEqual(ob.getTradeHistory(), expected);

    assert(!ob.hasOrder(1));
    assert(!ob.hasOrder(2));

    std::cout << "PASS: exact-price match\n";
}

void testFIFOSellOrdersAtSamePrice()
{
    OrderBook ob;

    ob.addOrder({1, false, 100.0, 5});
    ob.addOrder({2, false, 100.0, 5});
    ob.addOrder({3, false, 100.0, 5});

    ob.addOrder({4, true, 100.0, 7});

    std::vector<Trade> expected = {
        {4, 1, 100.0, 5},
        {4, 2, 100.0, 2}
    };

    assertTradesEqual(ob.getTradeHistory(), expected);

    assert(!ob.hasOrder(1));
    assert(ob.hasOrder(2));
    assert(ob.getOrderQuantity(2) == 3);
    assert(ob.hasOrder(3));
    assert(ob.getOrderQuantity(3) == 5);
    assert(!ob.hasOrder(4));

    std::cout << "PASS: FIFO for SELL orders at same price\n";
}

void testFIFOBuyOrdersAtSamePrice()
{
    OrderBook ob;

    ob.addOrder({1, true, 100.0, 5});
    ob.addOrder({2, true, 100.0, 5});
    ob.addOrder({3, true, 100.0, 5});

    ob.addOrder({4, false, 100.0, 7});

    std::vector<Trade> expected = {
        {1, 4, 100.0, 5},
        {2, 4, 100.0, 2}
    };

    assertTradesEqual(ob.getTradeHistory(), expected);

    assert(!ob.hasOrder(1));
    assert(ob.hasOrder(2));
    assert(ob.getOrderQuantity(2) == 3);
    assert(ob.hasOrder(3));
    assert(ob.getOrderQuantity(3) == 5);
    assert(!ob.hasOrder(4));

    std::cout << "PASS: FIFO for BUY orders at same price\n";
}

void testCancelMiddleOrder()
{
    OrderBook ob;

    ob.addOrder({1, true, 100.0, 5});
    ob.addOrder({2, true, 100.0, 5});
    ob.addOrder({3, true, 100.0, 5});

    ob.cancelOrder(2);

    assert(ob.hasOrder(1));
    assert(!ob.hasOrder(2));
    assert(ob.hasOrder(3));
    assert(ob.hasPriceLevel(true, 100.0));

    std::cout << "PASS: cancel middle order\n";
}

void testCancelOnlyOrderAtPrice()
{
    OrderBook ob;

    ob.addOrder({1, true, 100.0, 5});

    ob.cancelOrder(1);

    assert(!ob.hasOrder(1));
    assert(!ob.hasPriceLevel(true, 100.0));

    std::cout << "PASS: cancel only order at price\n";
}

void testCancelAfterPartialFill()
{
    OrderBook ob;

    ob.addOrder({1, false, 100.0, 10});
    ob.addOrder({2, true, 105.0, 6});

    std::vector<Trade> expected = {
        {2, 1, 100.0, 6}
    };

    assertTradesEqual(ob.getTradeHistory(), expected);

    assert(ob.hasOrder(1));
    assert(ob.getOrderQuantity(1) == 4);

    ob.cancelOrder(1);

    assert(!ob.hasOrder(1));
    assert(!ob.hasPriceLevel(false, 100.0));

    std::cout << "PASS: cancel after partial fill\n";
}

void testCancelNonexistentOrder()
{
    OrderBook ob;

    ob.addOrder({1, true, 100.0, 5});

    ob.cancelOrder(999);

    assert(ob.hasOrder(1));
    assert(ob.getOrderQuantity(1) == 5);

    std::vector<Trade> expected = {};

    assertTradesEqual(ob.getTradeHistory(), expected);

    std::cout << "PASS: cancel nonexistent order\n";
}

void testQuantityModification()
{
    OrderBook ob;

    ob.addOrder({1, true, 100.0, 10});
    ob.addOrder({2, true, 100.0, 10});
    ob.addOrder({3, true, 100.0, 10});

    ob.modifyOrder(2, 5, 100.0);

    assert(ob.hasOrder(1));
    assert(ob.hasOrder(2));
    assert(ob.hasOrder(3));
    assert(ob.getOrderQuantity(2) == 5);

    std::vector<Trade> expected = {};

    assertTradesEqual(ob.getTradeHistory(), expected);

    std::cout << "PASS: quantity modification\n";
}

void testPriceModification()
{
    OrderBook ob;

    ob.addOrder({1, true, 100.0, 10});

    ob.modifyOrder(1, 10, 105.0);

    assert(!ob.hasPriceLevel(true, 100.0));
    assert(ob.hasPriceLevel(true, 105.0));
    assert(ob.hasOrder(1));
    assert(ob.getOrderQuantity(1) == 10);

    std::vector<Trade> expected = {};

    assertTradesEqual(ob.getTradeHistory(), expected);

    std::cout << "PASS: price modification\n";
}

void testModificationTriggersMatch()
{
    OrderBook ob;

    ob.addOrder({1, false, 102.0, 5});
    ob.addOrder({2, true, 100.0, 5});

    ob.modifyOrder(2, 5, 105.0);

    std::vector<Trade> expected = {
        {2, 1, 102.0, 5}
    };

    assertTradesEqual(ob.getTradeHistory(), expected);

    assert(!ob.hasOrder(1));
    assert(!ob.hasOrder(2));

    std::cout << "PASS: modification triggers matching\n";
}

void testPriceModificationGetsNewFIFOPosition()
{
    OrderBook ob;

    ob.addOrder({1, true, 100.0, 5});
    ob.addOrder({2, true, 99.0, 5});

    ob.modifyOrder(2, 5, 100.0);

    ob.addOrder({3, false, 100.0, 5});

    std::vector<Trade> expected = {
        {1, 3, 100.0, 5}
    };

    assertTradesEqual(ob.getTradeHistory(), expected);

    assert(!ob.hasOrder(1));
    assert(!ob.hasOrder(3));

    // Modified order 2 moved to the back of the 100.0 price level.
    assert(ob.hasOrder(2));
    assert(ob.getOrderQuantity(2) == 5);

    std::cout << "PASS: price modification gets new FIFO position\n";
}

int main()
{
    testBuyFullyFillsSell();
    testBuyPartiallyFillsSell();
    testSellPartiallyFillsBuy();
    testBuyConsumesMultipleSellLevels();
    testSellConsumesMultipleBuyLevels();
    testNonCrossingBuyRests();
    testNonCrossingSellRests();
    testExactPriceMatch();
    testFIFOSellOrdersAtSamePrice();
    testFIFOBuyOrdersAtSamePrice();
    testCancelMiddleOrder();
    testCancelOnlyOrderAtPrice();
    testCancelAfterPartialFill();
    testCancelNonexistentOrder();
    testQuantityModification();
    testPriceModification();
    testModificationTriggersMatch();
    testPriceModificationGetsNewFIFOPosition();

    std::cout << "\nAll tests passed.\n";

    return 0;
}
