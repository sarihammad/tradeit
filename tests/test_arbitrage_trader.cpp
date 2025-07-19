#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "strategy/arbitrage_trader.hpp"
#include "core/order.hpp"
#include "core/trade.hpp"

using namespace strategy;
using namespace core;

TEST_CASE("ArbitrageTrader stops on max loss", "[arbitrage]") {
    std::vector<Order> submitted;

    ArbitrageTrader trader(
        "ETH-USD", "BTC-USD",
        [&](const Order& o) { submitted.push_back(o); },
        0.03, // spread threshold
        15,   // order size
        -100.0 // max loss
    );

    trader.start();

    // Simulate a large loss
    Trade losing_trade{1, 100, 101, "ETH-USD", 50.0, 1, 123456, Side::SELL};
    trader.onTrade(losing_trade); // should trigger PnL += 50.0
    Trade losing_trade_2{2, 102, 103, "ETH-USD", 200.0, 2, 123457, Side::SELL};
    trader.onTrade(losing_trade_2); // should bring PnL way below -100

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    trader.stop();

    REQUIRE(trader.riskViolated());
}

TEST_CASE("ArbitrageTrader updates position and PnL correctly", "arbitrage") {
    ArbitrageTrader trader(
        "ETH-USD", "BTC-USD",
        [](const Order&) {}, 
        0.03, 15, -1000.0
    );

    trader.start();

    // simulate ETH buy 
    Trade t1{1, 1, 2, "ETH-USD", 100.0, 2, 1234, Side::SELL}; // Short ETH
    trader.onTrade(t1);

    // simulate BTC sell 
    Trade t2{2, 3, 4, "BTC-USD", 101.0, 2, 1235, Side::BUY}; // long BTC
    trader.onTrade(t2);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    trader.stop();

    REQUIRE(trader.getPosition("ETH-USD") == -2);
    REQUIRE(trader.getPosition("BTC-USD") == 2);
    REQUIRE(trader.getRealizedPnL() == Catch::Approx(202.0 - 200.0)); // BTC - ETH
}

TEST_CASE("ArbitrageTrader ignores irrelevant trades", "[arbitrage]") {
    ArbitrageTrader trader(
        "ETH-USD", "BTC-USD",
        [](const Order&) {}, 0.03, 15, -1000.0
    );

    trader.start();

    Trade unrelated{1, 1, 2, "DOGE-USD", 10.0, 1, 1234, Side::BUY};
    trader.onTrade(unrelated);

    trader.stop();

    REQUIRE(trader.getPosition("ETH-USD") == 0);
    REQUIRE(trader.getPosition("BTC-USD") == 0);
    REQUIRE(trader.getRealizedPnL() == Catch::Approx(0.0));
}