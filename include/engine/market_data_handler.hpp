/**
 * @file market_data_handler.hpp
 * @brief Declares the MarketDataHandler class for feeding tick data to the system.
 */

#pragma once

#include "core/order.hpp"

#include <queue>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <string>
#include <atomic>
#include <thread>
#include <functional>

namespace engine {

/**
 * @class MarketDataHandler
 * @brief Simulates a market data feed by producing tick-level order events.
 *
 * Loads tick data from a file or generates synthetic data, then pushes orders
 * to a thread-safe queue that strategies can consume from.
 */
class MarketDataHandler {
public:
    using OrderCallback = std::function<void(core::Order)>;

    /**
     * @brief Constructs a MarketDataHandler.
     * @param file_path Path to the CSV file with tick data (optional)
     */
    explicit MarketDataHandler(const std::string& file_path = "");

    /**
     * @brief Starts the background thread that feeds data.
     * @param callback Function to call with each parsed order
     */
    void start(OrderCallback callback);

    /**
     * @brief Stops the background thread safely.
     */
    void stop();

    void setOrderCallback(OrderCallback cb);

    void load();

private:
    std::string file_path_;
    std::atomic<bool> running_;
    std::thread worker_;
    OrderCallback callback_;

    /**
     * @brief Internal method to read from CSV and emit orders.
     * Format: timestamp,symbol,side,price,quantity,type
     */
    void feedLoop(OrderCallback callback);

    /**
     * @brief Parses a CSV line into an Order.
     */
    core::Order parseLine(const std::string& line);
};

}