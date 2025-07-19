/**
 * @file market_maker.hpp
 * @brief Declares a simple market-making strategy.
 */

#pragma once

#include "strategy/strategy.hpp"
#include "engine/order_book.hpp"

#include <chrono>
#include <unordered_set>
#include <functional>
#include <fstream>

namespace strategy {

/**
 * @class MarketMaker
 * @brief A simple market-making strategy that places passive bid/ask orders near mid-price.
 */
class MarketMaker : public Strategy {
public:
    using SubmitOrderCallback = std::function<void(const core::Order&)>;

    /**
     * @brief Constructs a MarketMaker strategy.
     * @param symbol Instrument to trade (e.g., "ETH-USD")
     * @param book Reference to the order book
     * @param submit_fn Function to submit orders to the engine
     * @param max_loss Maximum loss threshold
     */
    explicit MarketMaker(
        const std::string& symbol,
        engine::OrderBook& book,
        SubmitOrderCallback submit,
        double max_loss);

    void start() override;
    void stop() override;
    void onMarketData(const core::Order& order) override;
    void onTrade(const core::Trade& trade) override;
    std::string name() const override;
    void printSummary() const override;
    void exportSummary(const std::string& path) const override;

    // Override reporting methods
    size_t totalTrades() const override { return total_trades_; }
    double averageTradeSize() const override {
        return total_trades_ > 0 ? static_cast<double>(total_quantity_) / total_trades_ : 0.0;
    }
    double maxDrawdown() const override { return max_drawdown_; }
    bool riskViolated() const override { return risk_violated_; }

private:
    std::string symbol_;
    engine::OrderBook& book_;
    SubmitOrderCallback submitOrder_;
    std::atomic<bool> running_;
    std::thread worker_;

    std::mutex market_mutex_;
    std::vector<core::Order> recent_market_orders_;
    uint64_t current_bid_id_ = 0;
    uint64_t current_ask_id_ = 0;

    std::atomic<uint64_t> order_id_counter_ = 1;

    std::unordered_map<uint64_t, core::Order> active_orders_;
    std::unordered_map<uint64_t, uint32_t> filled_quantity_;

    mutable std::mutex pnl_mutex_;
    int inventory_ = 0;
    int inventory_limit_ = 10;
    double realized_pnl_ = 0.0;
    double max_loss_;

    int max_inventory_ = 100;

    size_t total_quotes_ = 0;
    size_t total_trades_ = 0;

    // PnL tracking fields
    double peak_pnl_ = 0.0;
    double max_drawdown_ = 0.0;
    bool risk_violated_ = false;
    uint64_t total_quantity_ = 0;

    std::ofstream metrics_log_;
    std::ofstream trade_log_;

    void run();

    /**
     * @brief Creates symmetric bid/ask orders around mid-price.
     */
    void placeQuotes();

    /**
     * @brief Computes mid price from the order book.
     * @return Mid-price or -1 if no data available
     */
    double computeMidPrice();

    core::Order createOrder(core::Side side, double price, uint32_t qty, uint64_t ts);
    
 #ifdef UNIT_TESTING
public:
    /**
     * @brief Injects an active order into the internal active order map for testing purposes.
     * @param id The order ID
     * @param order The Order object to insert
     */
    void injectActiveOrder(uint64_t id, const core::Order& order) {
        active_orders_[id] = order;
    }
    /**
     * @brief Injects a filled quantity for a given order ID for testing purposes.
     * @param id The order ID
     * @param qty The filled quantity to set
     */
    void injectFilledQuantity(uint64_t id, uint32_t qty) {
        filled_quantity_[id] = qty;
    }
 #endif
};

}