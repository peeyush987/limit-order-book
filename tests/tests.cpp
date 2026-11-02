#include "CommandQueue.h"
#include "OrderBook.h"
#include "MatchingEngine.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include "lockFreeMPSCqueue.h"
#include "LockFreeMSQueue.h"


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

void testConcurrentAdds()
{
    OrderBook book;

    constexpr int NUM_THREADS = 4;
    constexpr int ORDERS_PER_THREAD = 250;

    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t)
    {
        threads.emplace_back([&, t]()
        {
            int startId = t * ORDERS_PER_THREAD;

            for (int i = 0; i < ORDERS_PER_THREAD; ++i)
            {
                int id = startId + i;

                book.addOrder({
                    id,
                    true,
                    100.00,
                    10
                });
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    for (int id = 0; id < NUM_THREADS * ORDERS_PER_THREAD; ++id)
    {
        assert(book.hasOrder(id));
    }

    std::cout << "Concurrent add test passed\n";
}

void testConcurrentReadsAndWrites()
{
    OrderBook book;

    constexpr int NUM_WRITERS = 2;
    constexpr int ORDERS_PER_WRITER = 500;

    std::vector<std::thread> threads;

    // Writers
    for (int t = 0; t < NUM_WRITERS; ++t)
    {
        threads.emplace_back([&, t]()
        {
            int startId = t * ORDERS_PER_WRITER;

            for (int i = 0; i < ORDERS_PER_WRITER; ++i)
            {
                int id = startId + i;

                book.addOrder({
                    id,
                    true,
                    100.00,
                    10
                });
            }
        });
    }

    // Reader
    threads.emplace_back([&]()
    {
        for (int i = 0; i < 5000; ++i)
        {
            book.hasOrder(i % (NUM_WRITERS * ORDERS_PER_WRITER));
            book.hasPriceLevel(true, 100.00);
        }
    });

    for (auto& thread : threads)
    {
        thread.join();
    }

    std::cout << "Concurrent read/write test passed\n";
}


void testCommandQueue()
{
    CommandQueue queue;

    std::thread consumer([&]()
    {
        std::optional<OrderCommand> command = queue.waitAndPop();

        if (!command)
        {
            throw std::runtime_error("Failed to receive command");
        }

        const auto& addCommand =
            std::get<AddOrderCommand>(*command);

        assert(addCommand.order.id == 42);
        assert(addCommand.order.isBuy);
        assert(addCommand.order.price == 100.00);
        assert(addCommand.order.quantity == 10);
    });

    // Give the consumer a chance to reach waitAndPop()
    // before the producer sends the command.
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)
    );

    std::thread producer([&]()
    {
        queue.push(AddOrderCommand{
            Order{42, true, 100.00, 10}
        });
    });

    producer.join();
    consumer.join();

    std::cout << "Command queue test passed\n";
}

void testMultipleProducers()
{
    CommandQueue queue;

    constexpr int NUM_PRODUCERS = 3;
    constexpr int COMMANDS_PER_PRODUCER = 100;
    constexpr int TOTAL_COMMANDS =
        NUM_PRODUCERS * COMMANDS_PER_PRODUCER;

    std::vector<std::thread> producers;

    for (int producerId = 0;
         producerId < NUM_PRODUCERS;
         ++producerId)
    {
        producers.emplace_back([&, producerId]()
        {
            int startId =
                producerId * COMMANDS_PER_PRODUCER;

            for (int i = 0; i < COMMANDS_PER_PRODUCER; ++i)
            {
                int id = startId + i;

                queue.push(AddOrderCommand{
                    Order{id, true, 100.00, 10}
                });
            }
        });
    }

    std::vector<bool> received(TOTAL_COMMANDS, false);

    for (int i = 0; i < TOTAL_COMMANDS; ++i)
    {
        std::optional<OrderCommand> command = queue.waitAndPop();

        if (!command)
        {
            throw std::runtime_error("Failed to receive command");
        }

        const auto& addCommand =
            std::get<AddOrderCommand>(*command);

        int id = addCommand.order.id;

        assert(id >= 0);
        assert(id < TOTAL_COMMANDS);
        assert(!received[id]);

        received[id] = true;
    }

    for (auto& producer : producers)
    {
        producer.join();
    }

    for (bool wasReceived : received)
    {
        assert(wasReceived);
    }

    std::cout << "Multiple producer test passed\n";
}

void testMPSCQueue()
{
    CommandQueue queue;

    constexpr int NUM_PRODUCERS = 3;
    constexpr int COMMANDS_PER_PRODUCER = 100;
    constexpr int TOTAL_COMMANDS =
        NUM_PRODUCERS * COMMANDS_PER_PRODUCER;

    std::vector<bool> received(TOTAL_COMMANDS, false);

    // One consumer.
    std::thread consumer([&]()
    {
        for (int i = 0; i < TOTAL_COMMANDS; ++i)
        {
            std::optional<OrderCommand> command = queue.waitAndPop();

            if (!command)
            {
                throw std::runtime_error("Failed to receive command");
            }

            const auto& addCommand =
                std::get<AddOrderCommand>(*command);

            int id = addCommand.order.id;

            assert(id >= 0);
            assert(id < TOTAL_COMMANDS);
            assert(!received[id]);

            received[id] = true;
        }
    });

    // Multiple producers.
    std::vector<std::thread> producers;

    for (int producerId = 0;
         producerId < NUM_PRODUCERS;
         ++producerId)
    {
        producers.emplace_back([&, producerId]()
        {
            int startId =
                producerId * COMMANDS_PER_PRODUCER;

            for (int i = 0;
                 i < COMMANDS_PER_PRODUCER;
                 ++i)
            {
                int id = startId + i;

                queue.push(AddOrderCommand{
                    Order{id, true, 100.00, 10}
                });
            }
        });
    }

    // Wait for producers.
    for (auto& producer : producers)
    {
        producer.join();
    }

    // Wait for the single consumer.
    consumer.join();

    // Every expected command must have been received.
    for (bool wasReceived : received)
    {
        assert(wasReceived);
    }

    std::cout << "MPSC queue test passed\n";
}

void testCommandQueueClose()
{
    CommandQueue queue;

    bool accepted1 = queue.push(AddOrderCommand{
        Order{1, true, 100.00, 10}
    });

    bool accepted2 = queue.push(AddOrderCommand{
        Order{2, true, 101.00, 20}
    });

    assert(accepted1);
    assert(accepted2);

    queue.close();

    bool accepted3 = queue.push(AddOrderCommand{
        Order{3, true, 102.00, 30}
    });

    assert(!accepted3);

    auto first = queue.waitAndPop();
    assert(first.has_value());
    assert(std::get<AddOrderCommand>(*first).order.id == 1);

    auto second = queue.waitAndPop();
    assert(second.has_value());
    assert(std::get<AddOrderCommand>(*second).order.id == 2);

    auto third = queue.waitAndPop();
    assert(!third.has_value());

    std::cout << "Command queue close test passed\n";
}

void testCloseWakesBlockedConsumer()
{
    CommandQueue queue;
    std::optional<OrderCommand> result;

    std::thread consumer([&]()
    {
        result = queue.waitAndPop();
    });

    // Give the consumer an opportunity to block on the empty queue.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    queue.close();

    consumer.join();

    assert(!result.has_value());

    std::cout << "Close wakes blocked consumer test passed\n";
}

void testMatchingEngineLifecycle()
{
    MatchingEngine engine;

    engine.start();

    bool accepted = engine.submit(
        AddOrderCommand{
            Order{42, true, 100.00, 10}
        }
    );

    assert(accepted);

    engine.stop();

    bool acceptedAfterStop = engine.submit(
        AddOrderCommand{
            Order{43, true, 101.00, 20}
        }
    );

    assert(!acceptedAfterStop);

    std::cout << "MatchingEngine lifecycle test passed\n";
}

void testMatchingEngineProcessesCommands()
{
    MatchingEngine engine;
    engine.start();

    // Submit an ADD command.
    bool addAccepted = engine.submit(
        AddOrderCommand{
            Order{42, true, 100.00, 10}
        }
    );

    assert(addAccepted);

    // Verify that the matching thread processed the ADD.
    bool existsAfterAdd = engine.hasOrder(42);
    assert(existsAfterAdd);

    // Submit a CANCEL command.
    bool cancelAccepted = engine.submit(
        CancelOrderCommand{42}
    );

    assert(cancelAccepted);

    // Verify that the matching thread processed the CANCEL.
    bool existsAfterCancel = engine.hasOrder(42);
    assert(!existsAfterCancel);

    engine.stop();

    std::cout << "MatchingEngine command-processing test passed\n";
}

void testConcurrentProducersThroughMatchingEngine()
{
    MatchingEngine engine;
    engine.start();

    constexpr int NUM_PRODUCERS = 3;
    constexpr int ORDERS_PER_PRODUCER = 100;
    constexpr int TOTAL_ORDERS =
        NUM_PRODUCERS * ORDERS_PER_PRODUCER;

    std::atomic<int> rejectedSubmissions{0};
    std::vector<std::thread> producers;

    for (int producerId = 0;
         producerId < NUM_PRODUCERS;
         ++producerId)
    {
        producers.emplace_back([&, producerId]()
        {
            int startId = producerId * ORDERS_PER_PRODUCER;

            for (int i = 0; i < ORDERS_PER_PRODUCER; ++i)
            {
                int orderId = startId + i;

                bool accepted = engine.submit(
                    AddOrderCommand{
                        Order{orderId, true, 100.00, 10}
                    }
                );

                if (!accepted)
                {
                    rejectedSubmissions.fetch_add(1);
                }
            }
        });
    }

    for (auto& producer : producers)
    {
        producer.join();
    }

    assert(rejectedSubmissions.load() == 0);

    for (int id = 0; id < TOTAL_ORDERS; ++id)
    {
        bool exists = engine.hasOrder(id);
        assert(exists);
    }

    engine.stop();

    std::cout
        << "Concurrent producer MatchingEngine test passed\n";
}

void testMatchingEngineMixedCommands()
{
    MatchingEngine engine;
    engine.start();

    bool addAccepted = engine.submit(
        AddOrderCommand{
            Order{42, true, 100.00, 10}
        }
    );
    assert(addAccepted);

    bool existsAfterAdd = engine.hasOrder(42);
    assert(existsAfterAdd);

    bool modifyAccepted = engine.submit(
        ModifyOrderCommand{42, 20, 99.50}
    );
    assert(modifyAccepted);

    bool existsAfterModify = engine.hasOrder(42);
    assert(existsAfterModify);

    int quantityAfterModify = engine.getOrderQuantity(42);
    assert(quantityAfterModify == 20);

    bool cancelAccepted = engine.submit(
        CancelOrderCommand{42}
    );
    assert(cancelAccepted);

    bool existsAfterCancel = engine.hasOrder(42);
    assert(!existsAfterCancel);

    engine.stop();

    std::cout << "MatchingEngine mixed-command test passed\n";
}

void testMatchingEngineStopsWhileIdle()
{
    MatchingEngine engine;

    engine.start();

    // No commands submitted.
    // The matching thread should be waiting on the empty queue.
    engine.stop();

    std::cout << "MatchingEngine idle shutdown test passed\n";
}

void testAtomicQueueBasicBehavior()
{
    LockFreeMPSCQueue<int, 4> queue;

    // Initially empty.
    auto empty = queue.tryPop();
    assert(!empty.has_value());

    // Fill all four slots.
    for (int value = 1; value <= 4; ++value)
    {
        bool pushed = queue.tryPush(int{value});
        assert(pushed);
    }

    // The queue is full, so another push should fail.
    bool fullRejected = !queue.tryPush(5);
    assert(fullRejected);

    // Remove the first item.
    auto first = queue.tryPop();
    assert(first.has_value());
    assert(*first == 1);

    // The released slot should now be reusable.
    bool pushedFive = queue.tryPush(5);
    assert(pushedFive);

    // Verify FIFO order, including the reused slot.
    for (int expected = 2; expected <= 5; ++expected)
    {
        auto result = queue.tryPop();

        assert(result.has_value());
        assert(*result == expected);
    }

    // All items have been removed.
    auto finalEmpty = queue.tryPop();
    assert(!finalEmpty.has_value());

    std::cout << "Atomic queue basic behavior test passed\n";
}

void testAtomicQueueConcurrentProducers()
{
    constexpr int NUM_PRODUCERS = 3;
    constexpr int ITEMS_PER_PRODUCER = 5000;
    constexpr int TOTAL_ITEMS =
        NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    LockFreeMPSCQueue<int, 1024> queue;

    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);

    // The consumer is the only thread that accesses this vector.
    std::vector<unsigned char> seen(TOTAL_ITEMS, 0);

    std::atomic<int> producersDone{0};

    int consumed = 0;
    bool valid = true;

    // Start three concurrent producers.
    for (int producerId = 0;
         producerId < NUM_PRODUCERS;
         ++producerId)
    {
        producers.emplace_back([&, producerId]()
        {
            const int firstId =
                producerId * ITEMS_PER_PRODUCER;

            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i)
            {
                const int value = firstId + i;

                // Retry when the bounded queue is full.
                while (!queue.tryPush(int{value}))
                {
                    std::this_thread::yield();
                }
            }

            producersDone.fetch_add(1, std::memory_order_release);
        });
    }

    // This thread is the single consumer.
    while (consumed < TOTAL_ITEMS)
    {
        auto result = queue.tryPop();

        if (result)
        {
            const int value = *result;

            if (value < 0 || value >= TOTAL_ITEMS)
            {
                valid = false;
            }
            else if (seen[value] != 0)
            {
                // We received the same item twice.
                valid = false;
            }
            else
            {
                seen[value] = 1;
            }

            ++consumed;
            continue;
        }

        // An empty result is temporary while producers are active.
        if (producersDone.load(std::memory_order_acquire)
            == NUM_PRODUCERS)
        {
            // All producers finished, but items are missing.
            valid = false;
            break;
        }

        std::this_thread::yield();
    }

    for (auto& producer : producers)
    {
        producer.join();
    }

    if (consumed != TOTAL_ITEMS)
    {
        valid = false;
    }

    for (unsigned char wasSeen : seen)
    {
        if (wasSeen == 0)
        {
            valid = false;
            break;
        }
    }

    assert(valid);

    std::cout
        << "Atomic queue concurrent-producer test passed\n";
}

void testLockFreeMSQueueConcurrentProducers()
{
    constexpr int NUM_PRODUCERS = 3;
    constexpr int ITEMS_PER_PRODUCER = 5000;
    constexpr int TOTAL_ITEMS =
        NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    LockFreeMSQueue<int> queue;

    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);

    // Only the consumer accesses these variables.
    std::vector<unsigned char> seen(TOTAL_ITEMS, 0);
    int consumed = 0;
    bool valid = true;

    std::atomic<int> producersDone{0};

    for (int producerId = 0;
         producerId < NUM_PRODUCERS;
         ++producerId)
    {
        producers.emplace_back([&, producerId]()
        {
            int firstId = producerId * ITEMS_PER_PRODUCER;

            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i)
            {
                int value = firstId + i;

                queue.tryPush(int{value});
            }

            producersDone.fetch_add(1, std::memory_order_release);
        });
    }

    // One consumer drains the queue.
    while (consumed < TOTAL_ITEMS)
    {
        auto result = queue.tryPop();

        if (result.has_value())
        {
            int value = *result;

            if (value < 0 || value >= TOTAL_ITEMS)
            {
                valid = false;
            }
            else if (seen[value] != 0)
            {
                valid = false;  // Duplicate value.
            }
            else
            {
                seen[value] = 1;
            }

            ++consumed;
        }
        else
        {
            if (producersDone.load(std::memory_order_acquire)
                == NUM_PRODUCERS)
            {
                valid = false;
                break;
            }

            std::this_thread::yield();
        }
    }

    for (auto& producer : producers)
    {
        producer.join();
    }

    if (consumed != TOTAL_ITEMS)
    {
        valid = false;
    }

    for (unsigned char wasSeen : seen)
    {
        if (wasSeen == 0)
        {
            valid = false;
            break;
        }
    }

    assert(valid);

    std::cout
        << "Lock-free MS queue concurrent test passed\n";
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
    testConcurrentAdds();
    testConcurrentReadsAndWrites();
    testCommandQueue();
    testMultipleProducers();
    testMPSCQueue();
    testCommandQueueClose();
    testCloseWakesBlockedConsumer();
    testMatchingEngineLifecycle();
    testMatchingEngineProcessesCommands();
    testConcurrentProducersThroughMatchingEngine();
    testMatchingEngineMixedCommands();
    testMatchingEngineStopsWhileIdle();
    testAtomicQueueBasicBehavior();
    testAtomicQueueConcurrentProducers();
    testLockFreeMSQueueConcurrentProducers();

    std::cout << "\nAll tests passed.\n";

    return 0;
}
