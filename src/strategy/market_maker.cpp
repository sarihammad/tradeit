/**
 * @file market_maker.cpp
 * @brief Implements the MarketMaker strategy logic.
 */

#include "strategy/market_maker.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <fstream>
#include <iomanip>

namespace strategy {
using namespace core;
using namespace std::chrono_literals;

MarketMaker::MarketMaker(
    const std::string& symbol,
    engine::OrderBook& book,
    SubmitOrderCallback submit,
    double max_loss)
    : inventory_limit_(10),
      symbol_(symbol),
      book_(book),
      submitOrder_(submit),
      running_(false),
      max_loss_(max_loss),
      current_bid_id_(0),
      current_ask_id_(0),
      total_quotes_(0),
      total_trades_(0) {}

void MarketMaker::start() {
    running_ = true;
    metrics_log_.open("logs/market_maker_metrics.csv");
    if (metrics_log_.is_open()) {
        metrics_log_ << "timestamp,inventory,pnl,spread,bid_id,ask_id\n";
    }
    trade_log_.open("logs/market_maker_trades.csv");
    if (trade_log_.is_open()) {
        trade_log_ << "trade_id,instrument,price,quantity,pnl,inventory,timestamp,risk_breached\n";
    }
    worker_ = std::thread(&MarketMaker::run, this);
}

void MarketMaker::stop() {
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
    if (metrics_log_.is_open()) {
        metrics_log_.close();
    }
    if (trade_log_.is_open()) {
        trade_log_.close();
    }

    std::cout << "[MarketMaker] Quotes: " << total_quotes_ << ", Trades: " << total_trades_
              << ", Quote-to-Trade Ratio: "
              << (total_trades_ > 0 ? static_cast<double>(total_quotes_) / total_trades_ : 0.0)
              << std::endl;
}

void MarketMaker::onMarketData(const Order& order) {
    if (order.instrument != symbol_) return;
    std::lock_guard<std::mutex> lock(market_mutex_);
    recent_market_orders_.push_back(order);
    if (recent_market_orders_.size() > 100) {
        recent_market_orders_.erase(recent_market_orders_.begin());
    }
}

void MarketMaker::onTrade(const Trade& trade) {
    if (trade.instrument != symbol_) return;

    total_trades_++;

    std::lock_guard<std::mutex> lock(pnl_mutex_);

    auto it = active_orders_.find(trade.buy_order_id);
    if (it != active_orders_.end()) {
        filled_quantity_[trade.buy_order_id] += trade.quantity;
        inventory_ += trade.quantity;
        realized_pnl_ -= trade.price * trade.quantity;
        total_quantity_ += trade.quantity;

        if (filled_quantity_[trade.buy_order_id] >= it->second.quantity) {
            active_orders_.erase(trade.buy_order_id);
            filled_quantity_.erase(trade.buy_order_id);
        }
    }

    it = active_orders_.find(trade.sell_order_id);
    if (it != active_orders_.end()) {
        filled_quantity_[trade.sell_order_id] += trade.quantity;
        inventory_ -= trade.quantity;
        realized_pnl_ += trade.price * trade.quantity;
        total_quantity_ += trade.quantity;

        if (filled_quantity_[trade.sell_order_id] >= it->second.quantity) {
            active_orders_.erase(trade.sell_order_id);
            filled_quantity_.erase(trade.sell_order_id);
        }
    }

    peak_pnl_ = std::max(peak_pnl_, realized_pnl_);
    double drawdown = peak_pnl_ - realized_pnl_;
    max_drawdown_ = std::max(max_drawdown_, drawdown);

    if (realized_pnl_ <= max_loss_ || std::abs(inventory_) > inventory_limit_) {
        std::cout << "[MarketMaker] Risk violation detected post-trade. Stopping strategy." << std::endl;
        risk_violated_ = true;
        running_ = false;
        return;
    }

    std::cout << "[MarketMaker] Inventory: " << inventory_
              << ", Realized P&L: " << realized_pnl_ << std::endl;

    // log trade to CSV
    if (trade_log_.is_open()) {
        double pnl = 0.0;
        if (active_orders_.find(trade.buy_order_id) != active_orders_.end()) {
            pnl = -trade.price * trade.quantity; // buy trade
        } else if (active_orders_.find(trade.sell_order_id) != active_orders_.end()) {
            pnl = trade.price * trade.quantity; // sell trade
        }
        
        trade_log_ << trade.trade_id << ","
                   << trade.instrument << ","
                   << trade.price << ","
                   << trade.quantity << ","
                   << pnl << ","
                   << inventory_ << ","
                   << trade.timestamp << ","
                   << (risk_violated_ ? "true" : "false") << "\n";
    }
}


std::string MarketMaker::name() const {
    return "MarketMaker";
}

void MarketMaker::run() {
    while (running_) {
        placeQuotes();
        std::this_thread::sleep_for(500ms); // refresh quote every 500ms
    }
}

void MarketMaker::placeQuotes() {
    // Order staleness and price drift thresholds
    const uint64_t max_age_us = 500'000; // 500ms
    const double max_price_drift = 0.02; // max drift allowed

    {
        std::lock_guard<std::mutex> lock(pnl_mutex_);
        if (realized_pnl_ <= max_loss_ || std::abs(inventory_) > inventory_limit_) {
            std::cout << "[MarketMaker] Risk limits exceeded. Stopping strategy." << std::endl;
            risk_violated_ = true;
            running_ = false;
            return;
        }
        risk_violated_ = false;
    }

    double mid = computeMidPrice();
    if (mid < 0) return;

    // Log spread if both best bid and ask are available
    auto best_bid = book_.getBestBid();
    auto best_ask = book_.getBestAsk();
    if (best_bid && best_ask) {
        double spread = best_ask->price - best_bid->price;
        std::cout << "Current spread: " << spread << std::endl;
    }

    double spread = (best_bid && best_ask) ? std::max(0.01, (best_ask->price - best_bid->price) / 2.0) : 0.05;
    double bid_price = mid - spread;
    double ask_price = mid + spread;

    uint32_t qty = 1;

    uint64_t ts = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count());

    // Timestamp and staleness check logic
    uint64_t now_us = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    auto cancel_if_stale = [&](uint64_t id, double new_price) {
        auto it = active_orders_.find(id);
        if (it == active_orders_.end()) return true;

        const Order& old = it->second;
        bool expired = now_us > old.timestamp + max_age_us;
        bool price_moved = std::abs(old.price - new_price) > max_price_drift;
        return expired || price_moved;
    };

    if (cancel_if_stale(current_bid_id_, bid_price)) {
        submitOrder_(Order(current_bid_id_, symbol_, OrderType::LIMIT, Side::BUY, 0.0, 0, 0));
        active_orders_.erase(current_bid_id_);
        filled_quantity_.erase(current_bid_id_);
        current_bid_id_ = 0;
    }

    if (cancel_if_stale(current_ask_id_, ask_price)) {
        submitOrder_(Order(current_ask_id_, symbol_, OrderType::LIMIT, Side::SELL, 0.0, 0, 0));
        active_orders_.erase(current_ask_id_);
        filled_quantity_.erase(current_ask_id_);
        current_ask_id_ = 0;
    }

    if (current_bid_id_ == 0) {
        Order bid = Order(order_id_counter_++, symbol_, OrderType::LIMIT, Side::BUY, bid_price, qty, ts);
        submitOrder_(bid);
        active_orders_[bid.id] = bid;
        filled_quantity_[bid.id] = 0;
        current_bid_id_ = bid.id;
    }

    if (current_ask_id_ == 0) {
        Order ask = Order(order_id_counter_++, symbol_, OrderType::LIMIT, Side::SELL, ask_price, qty, ts);
        submitOrder_(ask);
        active_orders_[ask.id] = ask;
        filled_quantity_[ask.id] = 0;
        current_ask_id_ = ask.id;
    }

    total_quotes_ += 2;

    if (metrics_log_.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        metrics_log_ << std::put_time(std::localtime(&now_c), "%F %T") << ","
                     << inventory_ << "," << realized_pnl_ << "," << spread << ","
                     << current_bid_id_ << "," << current_ask_id_ << "\n";
    }
}


double MarketMaker::computeMidPrice() {
    auto best_bid = book_.getBestBid();
    auto best_ask = book_.getBestAsk();

    if (!best_bid || !best_ask) {
        return -1.0; // cannot compute mid without both sides
    }

    return (best_bid->price + best_ask->price) / 2.0;
}

void MarketMaker::printSummary() const {
    std::cout << "\n[SUMMARY] Market Maker Strategy\n"
              << "[SUMMARY] Realized PnL: " << realized_pnl_ << "\n"
              << "[SUMMARY] Inventory [" << symbol_ << "]: " << inventory_ << "\n"
              << "[SUMMARY] Total Quotes: " << total_quotes_ << "\n"
              << "[SUMMARY] Total Trades: " << totalTrades() << "\n"
              << "[SUMMARY] Average Trade Size: " << averageTradeSize() << "\n"
              << "[SUMMARY] Quote-to-Trade Ratio: "
              << (totalTrades() > 0 ? static_cast<double>(total_quotes_) / totalTrades() : 0.0) << "\n"
              << "[SUMMARY] Max Drawdown: " << maxDrawdown() << "\n"
              << "[SUMMARY] Risk Breached: " << (riskViolated() ? "Yes" : "No") << "\n";
}

void MarketMaker::exportSummary(const std::string& path) const {
    std::ofstream out(path);
    out << "{\n";
    out << "  \"strategy\": \"marketmaker\",\n";
    out << "  \"pnl\": " << realized_pnl_ << ",\n";
    out << "  \"inventory_" << symbol_ << "\": " << inventory_ << ",\n";
    out << "  \"total_quotes\": " << total_quotes_ << ",\n";
    out << "  \"total_trades\": " << totalTrades() << ",\n";
    out << "  \"average_trade_size\": " << averageTradeSize() << ",\n";
    out << "  \"quote_to_trade_ratio\": " 
        << (totalTrades() > 0 ? static_cast<double>(total_quotes_) / totalTrades() : 0.0) << ",\n";
    out << "  \"max_drawdown\": " << maxDrawdown() << ",\n";
    out << "  \"risk_breached\": " << (riskViolated() ? "true" : "false") << "\n";
    out << "}\n";
    out.close();
}

// Unit test helper methods
#ifdef UNIT_TESTING
public:
    /**
     * @brief Injects a mock active order into the internal order map for testing purposes.
     */
    void injectActiveOrder(uint64_t id, const core::Order& order) {
        active_orders_[id] = order;
    }

    /**
     * @brief Injects a filled quantity for a given order ID for testing purposes.
     */
    void injectFilledQuantity(uint64_t id, uint32_t qty) {
        filled_quantity_[id] = qty;
    }
#endif

}