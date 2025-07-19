/**
 * @file order.hpp
 * @brief Defines order types, sides, and the Order data structure for trading.
 */

#pragma once

#include <string>
#include <cstdint>
#include <atomic>

namespace core {

/**
 * @enum OrderType
 * @brief Specifies whether an order is a limit or market order.
 */
enum class OrderType {
    MARKET,
    LIMIT
};

/**
 * @enum Side
 * @brief Indicates whether the order is a buy or a sell.
 */
enum class Side {
    BUY,
    SELL
};

/**
 * @struct Order
 * @brief Represents an order in the trading system.
 *
 * Orders are submitted by strategies and processed by the order book.
 */
struct Order {
    static std::atomic<uint64_t> global_order_id;

    uint64_t id;              // Unique order ID
    std::string instrument;   // Ticker symbol or instrument ID
    OrderType type;           // MARKET or LIMIT
    Side side;                // BUY or SELL
    double price;             // Price per unit (ignored for market orders)
    uint32_t quantity;        // Total number of units
    uint64_t timestamp;       // Epoch time in microseconds

    /**
     * @brief Default constructor
     */
    Order() = default;

    /**
     * @brief Constructs an Order with a custom ID (used for cancel requests).
     *
     * @param custom_id Custom order ID to use instead of generating a new one
     * @param instr Instrument symbol
     * @param t     Order type (MARKET or LIMIT)
     * @param s     Side (BUY or SELL)
     * @param p     Price per unit
     * @param q     Quantity of units
     * @param ts    Timestamp in microseconds
     */
    Order(uint64_t custom_id,
          const std::string& instr, OrderType t, Side s,
          double p, uint32_t q, uint64_t ts)
        : id(custom_id),
          instrument(instr),
          type(t),
          side(s),
          price(p),
          quantity(q),
          timestamp(ts) {}
};

}