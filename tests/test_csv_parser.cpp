#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "engine/market_data_handler.hpp"
#include "core/order.hpp"

using namespace core;
using namespace engine;

TEST_CASE("CSV Parsing - Valid Row", "[csv]") {
    MarketDataHandler handler("data/test_ticks.csv");

    std::vector<Order> parsed_orders;

    handler.setOrderCallback([&](const Order& o) {
        parsed_orders.push_back(o);
    });

    handler.load();

    REQUIRE(parsed_orders.size() == 3);

    const Order& order1 = parsed_orders[0];
    REQUIRE(order1.instrument == "ETH-USD");
    REQUIRE(order1.side == Side::BUY);
    REQUIRE(order1.price == Catch::Approx(1850.1));
    REQUIRE(order1.quantity == 2);
    REQUIRE(order1.timestamp == 1695500000000);
}

TEST_CASE("CSV Parsing - Malformed Row", "[csv]") {
    MarketDataHandler handler("data/bad_ticks.csv");

    std::vector<Order> parsed_orders;

    handler.setOrderCallback([&](const Order& o) {
        parsed_orders.push_back(o);
    });

    // should log a warning or skip bad rows
    handler.load();

    REQUIRE(parsed_orders.size() == 1);  // only one row should be good
}