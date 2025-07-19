/**
 * @file order_book.hpp
 * @brief Defines the Central Limit Order Book (CLOB) for matching buy/sell orders.
 */

#pragma once

#include "core/order.hpp"
#include "core/trade.hpp"

#include <map>
#include <deque>
#include <vector>
#include <mutex>
#include <functional>

namespace engine {

/**
 * @class OrderBook
 * @brief Central limit order book for a single instrument.
 *
 * Supports order insertion, matching, cancellation, and trade generation.
 */
class OrderBook {
public:
    explicit OrderBook(const std::string& instrument);

    /**
     * @brief Adds a new order to the book and attempts to match it.
     * @param order The incoming order (market or limit)
     * @return Vector of trades executed (may be empty)
     */
    std::vector<core::Trade> addOrder(const core::Order& order);

    /**
     * @brief Cancels an existing limit order (by ID).
     * @param order_id ID of the order to cancel
     * @return True if successfully canceled, false otherwise
     */
    bool cancelOrder(uint64_t order_id);

    /**
     * @brief Returns a const reference to the internal map of active orders.
     * @return Map of order ID to Order.
     */
    const std::unordered_map<uint64_t, core::Order>& getOrders() const;


    /**
     * @brief Prints the current state of the order book (for debugging/logging).
     */
    void printBook() const;

    /**
     * @brief Returns the best bid order (highest price buy), if any.
     */
    std::optional<core::Order> getBestBid() const;

    /**
     * @brief Returns the best ask order (lowest price sell), if any.
     */
    std::optional<core::Order> getBestAsk() const;
    
    void setTradeCallback(std::function<void(const core::Trade&)> cb);

private:
    std::string instrument_;
    mutable std::mutex mutex_;

    // Limit order storage: price -> deque of orders
    std::map<double, std::deque<core::Order>, std::greater<>> bids_; // Buy side
    std::map<double, std::deque<core::Order>> asks_;                 // Sell side

    // All active orders by ID
    std::unordered_map<uint64_t, core::Order> orders_;  // All active orders by ID

    // Trade ID tracker
    uint64_t next_trade_id_ = 1;

    std::function<void(const core::Trade&)> trade_callback_;

    /**
     * @brief Matches a market or aggressive limit order against the opposite book.
     * @param order Incoming order
     * @return List of trades resulting from matching
     */
    std::vector<core::Trade> match(const core::Order& order);

    /**
     * @brief Inserts a limit order into the correct side of the book.
     * @param order The limit order to insert
     */
    void insertLimitOrder(const core::Order& order);
};

}