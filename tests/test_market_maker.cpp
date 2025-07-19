#define CATCH_CONFIG_MAIN
#define UNIT_TESTING
#include <catch2/catch_test_macros.hpp>

#include "strategy/market_maker.hpp"
#include "engine/order_book.hpp"
#include "core/trade.hpp"

using namespace strategy;
using namespace core;
using namespace engine;

TEST_CASE("MarketMaker breaches max loss and stops", "marketmaker") {
    OrderBook dummy_book("ETH-USD");
    std::vector<Order> submitted;

    MarketMaker mm(
        "ETH-USD", dummy_book,
        [&](const Order& o) { submitted.push_back(o); },
        -50.0  // tight max loss
    );

    mm.start();

    // first trade (MarketMaker owns the BUY side only)
    Order dummy_buy_1{1, "ETH-USD", OrderType::LIMIT, Side::BUY, 100.0, 1, 12300};
    mm.injectActiveOrder(1, dummy_buy_1);
    mm.injectFilledQuantity(1, 0);
    mm.onTrade({1, 1, 999, "ETH-USD", 100.0, 1, 12345, Side::BUY});

    // second trade (MarketMaker owns the BUY side only)
    Order dummy_buy_2{2, "ETH-USD", OrderType::LIMIT, Side::BUY, 100.0, 1, 12300};
    mm.injectActiveOrder(2, dummy_buy_2);
    mm.injectFilledQuantity(2, 0);
    mm.onTrade({2, 2, 999, "ETH-USD", 100.0, 1, 12346, Side::BUY});

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mm.stop();

    REQUIRE(mm.riskViolated());
}

TEST_CASE("MarketMaker stops on inventory limit breach", "marketmaker") {
    OrderBook dummy_book("ETH-USD");
    std::vector<Order> submitted;

    MarketMaker mm(
        "ETH-USD", dummy_book,
        [&](const Order& o) { submitted.push_back(o); },
        -1000.0 // high loss threshold to isolate inventory test
    );

    mm.start();

    // first trade 
    Order dummy_buy_1{1, "ETH-USD", OrderType::LIMIT, Side::BUY, 50.0, 6, 12300};
    mm.injectActiveOrder(1, dummy_buy_1);
    mm.injectFilledQuantity(1, 0);
    mm.onTrade({1, 1, 999, "ETH-USD", 50.0, 6, 12345, Side::BUY});

    // second trade 
    Order dummy_buy_2{2, "ETH-USD", OrderType::LIMIT, Side::BUY, 51.0, 6, 12300};
    mm.injectActiveOrder(2, dummy_buy_2);
    mm.injectFilledQuantity(2, 0);
    mm.onTrade({2, 2, 999, "ETH-USD", 51.0, 6, 12346, Side::BUY});

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mm.stop();

    REQUIRE(mm.riskViolated());
}

TEST_CASE("MarketMaker logs quote activity", "marketmaker") {
    OrderBook dummy_book("ETH-USD");
    std::vector<Order> submitted;

    // stub best bid/ask for quote generation
    dummy_book.addOrder(Order{1, "ETH-USD", OrderType::LIMIT, Side::BUY, 99.0, 1, 1000});
    dummy_book.addOrder(Order{2, "ETH-USD", OrderType::LIMIT, Side::SELL, 101.0, 1, 1001});

    MarketMaker mm(
        "ETH-USD", dummy_book,
        [&](const Order& o) { submitted.push_back(o); },
        -9999.0
    );

    mm.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    mm.stop();

    REQUIRE(submitted.size() >= 2); // should have submitted at least a bid and ask
}