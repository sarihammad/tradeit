#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "strategy/momentum_trader.hpp"
#include "core/order.hpp"
#include "core/trade.hpp"

using namespace core;
using namespace strategy;

TEST_CASE("MomentumTrader reacts to momentum signal", "[momentum]") {
    std::vector<Order> submitted;

    MomentumTrader trader("ETH-USD",
        [&](const Order& o) { submitted.push_back(o); },
        -500.0);

    trader.onMarketData(Order{1, "ETH-USD", OrderType::LIMIT, Side::BUY, 100.0, 1, 1});
    trader.onMarketData(Order{2, "ETH-USD", OrderType::LIMIT, Side::BUY, 101.0, 1, 2});
    trader.onMarketData(Order{3, "ETH-USD", OrderType::LIMIT, Side::BUY, 103.0, 1, 3});

    trader.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    trader.stop();

    REQUIRE_FALSE(submitted.empty());
    REQUIRE(submitted.back().instrument == "ETH-USD");
    REQUIRE(submitted.back().type == OrderType::MARKET);
}

TEST_CASE("MomentumTrader stops on max loss breach", "[momentum]") {
    std::vector<Order> submitted;
    MomentumTrader trader("ETH-USD",
        [&](const Order& o) { submitted.push_back(o); },
        -10.0);

    trader.start();

    Trade losing_trade{1, 100, 101, "ETH-USD", 100.0, 1, 123456, Side::SELL};
    trader.onTrade(losing_trade);
    trader.onTrade(losing_trade);
    trader.onTrade(losing_trade);  // Accumulate loss

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    trader.stop();

    REQUIRE(trader.riskViolated());
}

TEST_CASE("MomentumTrader avoids action if insufficient data", "[momentum]") {
    std::vector<Order> submitted;
    MomentumTrader trader("ETH-USD",
        [&](const Order& o) { submitted.push_back(o); },
        -500.0);

    trader.onMarketData(Order{1, "ETH-USD", OrderType::LIMIT, Side::BUY, 100.0, 1, 1});
    trader.onMarketData(Order{2, "ETH-USD", OrderType::LIMIT, Side::BUY, 101.0, 1, 2});

    trader.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    trader.stop();

    REQUIRE(submitted.empty());  // not enough data to act
}