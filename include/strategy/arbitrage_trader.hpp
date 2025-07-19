/**
 * @file arbitrage_trader.hpp
 * @brief Defines a simple arbitrage strategy that trades on price inefficiencies.
 */

#pragma once

#include "strategy/strategy.hpp"
#include "core/order.hpp"
#include "core/trade.hpp"

#include <unordered_map>
#include <string>
#include <atomic>
#include <mutex>
#include <fstream>

namespace strategy {

class ArbitrageTrader : public Strategy {
public:
    ArbitrageTrader(const std::string& asset1,
                const std::string& asset2,
                SubmitOrderCallback submit,
                double spread,
                int order_size,
                double max_loss);

    void onMarketData(const core::Order& order) override;
    void onTrade(const core::Trade& trade) override;
    void start() override;
    void stop() override;
    std::string name() const override { return "ArbitrageTrader"; }
    void printSummary() const override;
    void exportSummary(const std::string& path) const override;

    size_t totalTrades() const override { return total_trades_; }
    double averageTradeSize() const override {
        return total_trades_ > 0 ? static_cast<double>(total_quantity_) / total_trades_ : 0.0;
    }
    double maxDrawdown() const override { return max_drawdown_; }
    bool riskViolated() const override { return risk_violated_; }

    int getPosition(const std::string& symbol) const {
    auto it = positions_.find(symbol);
    return it != positions_.end() ? it->second : 0;
}

double getRealizedPnL() const {
    return realized_pnl_;
}

private:
    std::string symbol1_;
    std::string symbol2_;
    SubmitOrderCallback submit_;
    double spread_;
    int order_size_;
    double max_loss_;
    std::atomic<bool> running_;
    std::mutex mutex_;

    std::unordered_map<std::string, double> best_bid_;
    std::unordered_map<std::string, double> best_ask_;

    double realized_pnl_ = 0.0;
    std::unordered_map<std::string, int> positions_;

    std::ofstream trade_log_;

    size_t total_trades_ = 0;
    uint64_t total_quantity_ = 0;
    double peak_pnl_ = 0.0;
    double max_drawdown_ = 0.0;
    bool risk_violated_ = false;

    void checkArbitrageOpportunity();
};

}