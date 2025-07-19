/**
 * @file momentum_trader.hpp
 * @brief Declares a simple momentum-based trading strategy.
 */

#pragma once

#include "strategy/strategy.hpp"
#include "engine/order_book.hpp"

#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <fstream>

namespace strategy {

/**
 * @class MomentumTrader
 * @brief A trading strategy that reacts to short-term price momentum.
 */
class MomentumTrader : public Strategy {
public:
    using SubmitOrderCallback = std::function<void(const core::Order&)>;

    explicit MomentumTrader(
        const std::string& symbol,
        SubmitOrderCallback submit_fn,
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
    SubmitOrderCallback submitOrder_;
    std::atomic<bool> running_;
    std::thread worker_;

    mutable std::mutex data_mutex_;
    std::vector<double> recent_prices_;  // e.g., last N mid prices

    uint64_t cooldown_end_ts_ = 0;

    int position_ = 0;
    double realized_pnl_ = 0.0;
    double max_loss_;

    // PnL tracking fields
    double peak_pnl_ = 0.0;
    double max_drawdown_ = 0.0;
    bool risk_violated_ = false;
    size_t total_trades_ = 0;
    uint64_t total_quantity_ = 0;

    std::ofstream trade_log_;

    void run();
    void evaluateMomentum();
    double getLatestPrice() const;
    uint64_t nowMicros() const;
};

}
