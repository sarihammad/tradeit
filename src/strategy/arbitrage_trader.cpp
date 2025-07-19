/**
 * @file arbitrage_trader.cpp
 * @brief Implements a basic arbitrage strategy between two instruments.
 */

#include "strategy/arbitrage_trader.hpp"
#include <iostream>
#include <cmath>
#include <fstream>

namespace strategy {

using namespace core;

ArbitrageTrader::ArbitrageTrader(const std::string& asset1,
                                 const std::string& asset2,
                                 SubmitOrderCallback submit,
                                 double spread,
                                 int order_size,
                                 double max_loss)
    : symbol1_(asset1),
      symbol2_(asset2),
      submit_(submit),
      spread_(spread),
      order_size_(order_size),
      running_(false),
      max_loss_(max_loss) {}

void ArbitrageTrader::start() {
    running_ = true;
    std::cout << "[ArbitrageTrader] Started arbitrage between " << symbol1_ << " and " << symbol2_ << std::endl;
    trade_log_.open("logs/arbitrage_trades.csv");
    trade_log_ << "trade_id,instrument,price,quantity,pnl,position_" << symbol1_
               << ",position_" << symbol2_ << ",total_pnl,risk_breached,timestamp\n";
}

void ArbitrageTrader::stop() {
    running_ = false;
    std::cout << "[ArbitrageTrader] Stopped." << std::endl;
    if (trade_log_.is_open()) {
        trade_log_.close();
    }
}

void ArbitrageTrader::onMarketData(const Order& order) {
    if (!running_) return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (order.side == Side::BUY) {
        best_bid_[order.instrument] = std::max(best_bid_[order.instrument], order.price);
    } else {
        if (best_ask_.count(order.instrument) == 0 || order.price < best_ask_[order.instrument]) {
            best_ask_[order.instrument] = order.price;
        }
    }

    checkArbitrageOpportunity();
}

void ArbitrageTrader::onTrade(const Trade& trade) {
    if (!running_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    double pnl = 0.0;

    if (trade.instrument == symbol1_) {
        int qty = (trade.side == Side::BUY) ? trade.quantity : -trade.quantity;
        positions_[symbol1_] += qty;
        pnl = qty * trade.price; // BUY = +PnL, SELL = -PnL
    } else if (trade.instrument == symbol2_) {
        int qty = (trade.side == Side::BUY) ? trade.quantity : -trade.quantity;
        positions_[symbol2_] += qty;
        pnl = qty * trade.price;
    }

    realized_pnl_ += pnl;

    total_trades_++;
    total_quantity_ += trade.quantity;

    // track PnL and drawdown
    peak_pnl_ = std::max(peak_pnl_, realized_pnl_);
    double drawdown = peak_pnl_ - realized_pnl_;
    max_drawdown_ = std::max(max_drawdown_, drawdown);

    if (realized_pnl_ < max_loss_) {
        risk_violated_ = true;
        stop();
    }

    std::cout << "[ArbitrageTrader] Trade received: "
              << "ID " << trade.trade_id
              << ", " << trade.instrument
              << ", Price: " << trade.price
              << ", Qty: " << trade.quantity
              << ", PnL: " << pnl
              << ", Position[" << symbol1_ << "]: " << positions_[symbol1_]
              << ", Position[" << symbol2_ << "]: " << positions_[symbol2_]
              << ", Total PnL: " << realized_pnl_
              << std::endl;

    if (trade_log_.is_open()) {
        trade_log_ << trade.trade_id << ","
                   << trade.instrument << ","
                   << trade.price << ","
                   << trade.quantity << ","
                   << pnl << ","
                   << positions_[symbol1_] << ","
                   << positions_[symbol2_] << ","
                   << realized_pnl_ << ","
                   << (risk_violated_ ? "true" : "false") << ","
                   << trade.timestamp << "\n";
    }
}

void ArbitrageTrader::checkArbitrageOpportunity() {
    if (!best_ask_.count(symbol1_) || !best_bid_.count(symbol2_)) return;
    if (!best_ask_.count(symbol2_) || !best_bid_.count(symbol1_)) return;

    double ask1 = best_ask_[symbol1_];
    double bid2 = best_bid_[symbol2_];
    double ask2 = best_ask_[symbol2_];
    double bid1 = best_bid_[symbol1_];

    uint64_t now_us = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    double threshold = 0.05;  // Minimum profit spread

    // Trade 1 -> Buy symbol1, sell symbol2
    if ((bid2 - ask1) > threshold) {
        submit_(Order(core::Order::global_order_id++, symbol1_, OrderType::LIMIT, Side::BUY, ask1, 10, now_us));
        submit_(Order(core::Order::global_order_id++, symbol2_, OrderType::LIMIT, Side::SELL, bid2, 10, now_us));
        std::cout << "[Arbitrage] Buy " << symbol1_ << " @ " << ask1 << ", Sell " << symbol2_ << " @ " << bid2 << std::endl;
    }

    // Trade 2 -> Buy symbol2, sell symbol1
    if ((bid1 - ask2) > threshold) {
        submit_(Order(core::Order::global_order_id++, symbol2_, OrderType::LIMIT, Side::BUY, ask2, 10, now_us));
        submit_(Order(core::Order::global_order_id++, symbol1_, OrderType::LIMIT, Side::SELL, bid1, 10, now_us));
        std::cout << "[Arbitrage] Buy " << symbol2_ << " @ " << ask2 << ", Sell " << symbol1_ << " @ " << bid1 << std::endl;
    }
}

void ArbitrageTrader::printSummary() const {
    std::cout << "\n[SUMMARY] Arbitrage Strategy\n"
              << "[SUMMARY] Realized PnL: " << realized_pnl_ << "\n"
              << "[SUMMARY] Position [" << symbol1_ << "]: " << (positions_.count(symbol1_) ? positions_.at(symbol1_) : 0) << "\n"
              << "[SUMMARY] Position [" << symbol2_ << "]: " << (positions_.count(symbol2_) ? positions_.at(symbol2_) : 0) << "\n"
              << "[SUMMARY] Total Trades: " << totalTrades() << "\n"
              << "[SUMMARY] Average Trade Size: " << averageTradeSize() << "\n"
              << "[SUMMARY] Max Drawdown: " << maxDrawdown() << "\n"
              << "[SUMMARY] Risk Breached: " << (riskViolated() ? "Yes" : "No") << "\n";
}

void ArbitrageTrader::exportSummary(const std::string& path) const {
    std::ofstream out(path);
    out << "{\n";
    out << "  \"strategy\": \"arbitrage\",\n";
    out << "  \"pnl\": " << realized_pnl_ << ",\n";
    out << "  \"position_" << symbol1_ << "\": " << (positions_.count(symbol1_) ? positions_.at(symbol1_) : 0) << ",\n";
    out << "  \"position_" << symbol2_ << "\": " << (positions_.count(symbol2_) ? positions_.at(symbol2_) : 0) << ",\n";
    out << "  \"total_trades\": " << totalTrades() << ",\n";
    out << "  \"average_trade_size\": " << averageTradeSize() << ",\n";
    out << "  \"max_drawdown\": " << maxDrawdown() << ",\n";
    out << "  \"risk_breached\": " << (riskViolated() ? "true" : "false") << "\n";
    out << "}\n";
    out.close();
}

} 