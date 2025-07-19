#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "engine/order_book.hpp"
#include "core/order.hpp"
#include "core/trade.hpp"

using namespace core;
using namespace engine;

TEST_CASE("OrderBook - Simple Buy/Sell Match", "[orderbook]") {
    OrderBook book("ETH-USD");

    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) {
        trades.push_back(t);
    });

    // add a sell order to book (existing liquidity)
    Order sell_order(Order::global_order_id++, "ETH-USD", OrderType::LIMIT, Side::SELL, 100.0, 2, 1000000);
    book.addOrder(sell_order);

    // add a buy order that matches the sell
    Order buy_order(Order::global_order_id++, "ETH-USD", OrderType::LIMIT, Side::BUY, 101.0, 1, 1000100);
    book.addOrder(buy_order);

    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].price == Catch::Approx(100.0));
    REQUIRE(trades[0].quantity == 1);
    REQUIRE(trades[0].instrument == "ETH-USD");

    // check that remaining quantity is still in book
    auto best_sell = book.getBestAsk();
    REQUIRE(best_sell);
    REQUIRE(best_sell->quantity == 1);
}

TEST_CASE("OrderBook - No Match", "[orderbook]") {
    OrderBook book("BTC-USD");

    bool trade_occurred = false;
    book.setTradeCallback([&](const Trade&) {
        trade_occurred = true;
    });

    // add buy and sell orders that don’t cross
    book.addOrder(Order(Order::global_order_id++, "BTC-USD", OrderType::LIMIT, Side::BUY, 29900.0, 1, 123));
    book.addOrder(Order(Order::global_order_id++, "BTC-USD", OrderType::LIMIT, Side::SELL, 30100.0, 1, 124));

    REQUIRE_FALSE(trade_occurred);

    auto bid = book.getBestBid();
    auto ask = book.getBestAsk();
    REQUIRE(bid);
    REQUIRE(ask);
    REQUIRE(bid->price < ask->price);
}

TEST_CASE("OrderBook - Market Order Match", "[orderbook]") {
    OrderBook book("ETH-USD");

    std::vector<Trade> trades;
    book.setTradeCallback([&](const Trade& t) {
        trades.push_back(t);
    });

    // add sell liquidity
    book.addOrder(Order(Order::global_order_id++, "ETH-USD", OrderType::LIMIT, Side::SELL, 200.0, 2, 2000000));

    // market buy order should match immediately
    book.addOrder(Order(Order::global_order_id++, "ETH-USD", OrderType::MARKET, Side::BUY, 0.0, 2, 2000010));

    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].quantity == 2);
    REQUIRE(trades[0].price == Catch::Approx(200.0));
}