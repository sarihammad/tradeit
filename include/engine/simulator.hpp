/**
 * @file simulator.hpp
 * @brief Defines the simulated exchange driver that processes orders and emits trades.
 */

#pragma once

#include "core/order.hpp"
#include "core/trade.hpp"
#include "engine/order_book.hpp"
#include "strategy/strategy.hpp"

#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>

namespace engine {

/**
 * @class Simulator
 * @brief Handles market data replay, order matching, and trade distribution.
 */
class Simulator {
public:
    /**
     * @brief Registers a strategy to receive trades and market updates.
     * @param strategy Pointer to a Strategy instance
     */
    void registerStrategy(std::shared_ptr<strategy::Strategy> strategy);

    /**
     * @brief Feeds an order into the simulator (from market data or strategy).
     * @param order Order to process
     */
    void onOrder(const core::Order& order);

    /**
     * @brief Starts all registered strategies.
     */
    void start();

    /**
     * @brief Stops all registered strategies.
     */
    void stop();

private:
    std::unordered_map<std::string, engine::OrderBook> books_; ///< Order books per instrument
    std::vector<std::shared_ptr<strategy::Strategy>> strategies_; ///< All trading strategies
    std::mutex mutex_; ///< Protect shared state
};

}  