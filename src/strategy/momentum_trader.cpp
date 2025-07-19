/**
 * @file momentum_trader.cpp
 * @brief Implements the MomentumTrader strategy logic.
 */

#include "strategy/momentum_trader.hpp"
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>

namespace strategy {
using namespace core;
using namespace std::chrono_literals;

MomentumTrader::MomentumTrader(
    const std::string& symbol,
    SubmitOrderCallback submit,
    double max_loss)
    : symbol_(symbol),
      submitOrder_(submit),
      running_(false),
      max_loss_(max_loss) {}

void MomentumTrader::start() {
    running_ = true;
    trade_log_.open("logs/momentum_trades.csv");
    if (trade_log_.is_open()) {
        trade_log_ << "trade_id,instrument,price,quantity,pnl,position,timestamp,risk_breached";
    }
    worker_ = std::thread(&MomentumTrader::run, this);
}

void MomentumTrader::stop() {
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
    if (trade_log_.is_open()) {
        trade_log_.close();
    }
}

void MomentumTrader::onMarketData(const Order& order) {
    if (order.instrument != symbol_) return;

    std::lock_guard<std::mutex> lock(data_mutex_);
    recent_prices_.push_back(order.price);
    if (recent_prices_.size() > 5) {
        recent_prices_.erase(recent_prices_.begin());
    }
}

void MomentumTrader::onTrade(const Trade& trade) {
    if (trade.instrument != symbol_) return;

    int qty = (trade.buy_order_id < trade.sell_order_id) ? trade.quantity : -trade.quantity;
    position_ += qty;
    double pnl = qty * trade.price * -1; // sell is +PnL, buy is -PnL

    realized_pnl_ += pnl;
    total_trades_++;
    total_quantity_ += trade.quantity;

    peak_pnl_ = std::max(peak_pnl_, realized_pnl_);
    double drawdown = peak_pnl_ - realized_pnl_;
    max_drawdown_ = std::max(max_drawdown_, drawdown);

    if (realized_pnl_ < max_loss_) {
        risk_violated_ = true;
        stop();
    }
    
    // Log trade to CSV
    if (trade_log_.is_open()) {
        trade_log_ << trade.trade_id << ","
                   << trade.instrument << ","
                   << trade.price << ","
                   << trade.quantity << ","
                   << pnl << ","
                   << position_ << ","
                   << trade.timestamp << ","
                   << (risk_violated_ ? "true" : "false") << "\n";
    }
}

std::string MomentumTrader::name() const {
    return "MomentumTrader";
}

void MomentumTrader::run() {
    while (running_) {
        evaluateMomentum();
        std::this_thread::sleep_for(200ms);  // Check every 200ms
    }
}

void MomentumTrader::evaluateMomentum() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    if (recent_prices_.size() < 3) return;

    // Simple momentum logic: last price > average of previous?
    double current = recent_prices_.back();
    double average = 0.0;
    for (size_t i = 0; i < recent_prices_.size() - 1; ++i) {
        average += recent_prices_[i];
    }
    average /= (recent_prices_.size() - 1);

    uint64_t now = nowMicros();
    if (now < cooldown_end_ts_) return;  // still cooling down

    Side action = (current > average) ? Side::BUY : Side::SELL;
    double price = current;
    uint32_t qty = 1;

    Order order(Order::global_order_id++, symbol_, OrderType::MARKET, action, price, qty, now);    submitOrder_(order);

    cooldown_end_ts_ = now + 1'000'000; // 1 second cooldown
}

double MomentumTrader::getLatestPrice() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return recent_prices_.empty() ? -1.0 : recent_prices_.back();
}

uint64_t MomentumTrader::nowMicros() const {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count());
}

void MomentumTrader::printSummary() const {
    std::cout << "[SUMMARY] Momentum Strategy\n"
              << "[SUMMARY] Realized PnL: " << realized_pnl_ << "\n"
              << "[SUMMARY] Position [" << symbol_ << "]: " << position_ << "\n"
              << "[SUMMARY] Total Trades: " << totalTrades() << "\n"
              << "[SUMMARY] Average Trade Size: " << averageTradeSize() << "\n"
              << "[SUMMARY] Max Drawdown: " << maxDrawdown() << "\n"
              << "[SUMMARY] Risk Breached: " << (riskViolated() ? "Yes" : "No") << "\n";
}

void MomentumTrader::exportSummary(const std::string& path) const {
    std::ofstream out(path);
    out << "{\n";
    out << "  \"strategy\": \"momentum\",\n";
    out << "  \"pnl\": " << realized_pnl_ << ",\n";
    out << "  \"position_" << symbol_ << "\": " << position_ << ",\n";
    out << "  \"total_trades\": " << totalTrades() << ",\n";
    out << "  \"average_trade_size\": " << averageTradeSize() << ",\n";
    out << "  \"max_drawdown\": " << maxDrawdown() << ",\n";
    out << "  \"risk_breached\": " << (riskViolated() ? "true" : "false") << "\n";
    out << "}\n";
    out.close();
}

}