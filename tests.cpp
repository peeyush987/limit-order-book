#include "OrderBook.h"
#include <cassert>
#include <iostream>

void testBuyFullyFillsSell()
{
    OrderBook ob;

    ob.addOrder({1, false, 100.0, 5});
    ob.addOrder({2, true, 105.0, 5});

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

    assert(ob.hasOrder(1));
    assert(!ob.hasOrder(2));

    std::cout << "PASS: BUY partially fills SELL\n";
}

void testSellPartiallyFillsBuy()
{
    OrderBook ob;

    ob.addOrder({1, true, 105.0, 10});
    ob.addOrder({2, false, 100.0, 6});

    assert(ob.hasOrder(1));
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

    assert(!ob.hasOrder(1));
    assert(!ob.hasOrder(2));
    assert(ob.hasOrder(3));
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

    assert(!ob.hasOrder(1));
    assert(!ob.hasOrder(2));
    assert(ob.hasOrder(3));
    assert(!ob.hasOrder(4));

    std::cout << "PASS: SELL consumes multiple BUY levels\n";
}

void testNonCrossingBuyRests()
{
    OrderBook ob;

    ob.addOrder({1, false, 105.0, 5});
    ob.addOrder({2, true, 100.0, 10});

    assert(ob.hasOrder(1));
    assert(ob.hasOrder(2));

    std::cout << "PASS: non-crossing BUY rests\n";
}

void testNonCrossingSellRests()
{
    OrderBook ob;

    ob.addOrder({1, true, 100.0, 5});
    ob.addOrder({2, false, 105.0, 10});

    assert(ob.hasOrder(1));
    assert(ob.hasOrder(2));

    std::cout << "PASS: non-crossing SELL rests\n";
}

void testExactPriceMatch()
{
    OrderBook ob;

    ob.addOrder({1, false, 100.0, 5});
    ob.addOrder({2, true, 100.0, 5});

    assert(!ob.hasOrder(1));
    assert(!ob.hasOrder(2));

    std::cout << "PASS: exact-price match\n";
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

    std::cout << "PASS: price modification\n";
}

void testModificationTriggersMatch()
{
    OrderBook ob;

    ob.addOrder({1, false, 102.0, 5});
    ob.addOrder({2, true, 100.0, 5});

    ob.modifyOrder(2, 5, 105.0);

    assert(!ob.hasOrder(1));
    assert(!ob.hasOrder(2));

    std::cout << "PASS: modification triggers matching\n";
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
    testCancelMiddleOrder();
    testCancelOnlyOrderAtPrice();
    testQuantityModification();
    testPriceModification();
    testModificationTriggersMatch();

    std::cout << "\nAll tests passed.\n";

    return 0;
}