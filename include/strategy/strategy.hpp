/**
 * @file strategy.hpp
 * @brief Defines the IStrategy interface for trading strategies.
 */

#pragma once

#include "core/order.hpp"
#include "core/trade.hpp"

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>

namespace strategy {

// callback for submitting an order to the exchange
using SubmitOrderCallback = std::function<void(const core::Order&)>;

/**
 * @class Strategy
 * @brief Abstract base class for trading strategies.
 *
 * Each strategy processes market data and submits orders. Designed to be run in its own thread.
 */
class Strategy {
public:
    virtual ~Strategy() = default;

    /**
     * @brief Starts the strategy’s processing loop.
     */
    virtual void start() = 0;

    /**
     * @brief Stops the strategy’s loop and joins its thread.
     */
    virtual void stop() = 0;

    /**
     * @brief Receives market data (e.g., a new tick or order book update).
     * @param order Incoming order from the market data feed
     */
    virtual void onMarketData(const core::Order& order) = 0;

    /**
     * @brief Optionally handle executed trades (e.g., for P&L).
     * @param trade Executed trade reported by engine
     */
    virtual void onTrade(const core::Trade& trade) = 0;

    /**
     * @brief Gets the name of the strategy.
     * @return Name as a string
     */
    virtual std::string name() const = 0;

    /**
     * @brief Prints a summary of the strategy's performance or state.
     */
    virtual void printSummary() const = 0;

    /**
     * @brief Exports a summary of the strategy to a file.
     * @param path Path to the output file
     */
    virtual void exportSummary(const std::string& path) const = 0;

    /**
     * @brief Returns the total number of trades executed by the strategy.
     * @return Total trade count
     */
    virtual size_t totalTrades() const { return 0; }

    /**
     * @brief Returns the average size of trades executed by the strategy.
     * @return Average trade size
     */
    virtual double averageTradeSize() const { return 0.0; }

    /**
     * @brief Returns the maximum drawdown experienced by the strategy.
     * @return Maximum drawdown value
     */
    virtual double maxDrawdown() const { return 0.0; }

    /**
     * @brief Indicates whether the strategy has violated its risk limits.
     * @return True if risk limits are violated, false otherwise
     */
    virtual bool riskViolated() const { return false; }
};

}