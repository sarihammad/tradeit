#include <catch2/catch_test_macros.hpp>
#include "engine/order_book.hpp"
#include "strategy/market_maker.hpp"
#include "core/trade.hpp"
#include <thread>
#include <chrono>

using namespace core;
using namespace engine;
using namespace strategy;

TEST_CASE("MarketMaker integration test with live engine", "integration") {
    OrderBook book("ETH-USD");
    std::vector<Order> submitted;

    MarketMaker mm(
        "ETH-USD", book,
        [&](const Order& o) { 
            submitted.push_back(o);
            book.addOrder(o); // simulate engine submitting to order book
        },
        -50.0 // max loss
    );

    book.setTradeCallback([&](const Trade& t) {
        mm.onTrade(t);
    });

    book.addOrder(Order{1, "ETH-USD", OrderType::LIMIT, Side::BUY, 99.0, 1, 12300});
    book.addOrder(Order{2, "ETH-USD", OrderType::LIMIT, Side::SELL, 101.0, 1, 12300});
    mm.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // allow quotes to form

    REQUIRE(submitted.size() >= 2); // bid + ask

    // simulate external market participant crossing the spread
    auto orders = book.getOrders();
    for (const auto& [id, order] : orders) {
        if (order.side == Side::BUY) {
            book.addOrder(Order{999, "ETH-USD", OrderType::LIMIT, Side::SELL, order.price, 1, 99999});
        } else {
            book.addOrder(Order{998, "ETH-USD", OrderType::LIMIT, Side::BUY, order.price, 1, 99998});
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // allow trade to be processed

    mm.stop();

    REQUIRE(mm.totalTrades() >= 1);
    REQUIRE(mm.averageTradeSize() > 0);
}