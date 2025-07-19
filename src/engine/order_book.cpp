/**
 * @file OrderBook.cpp
 * @brief Implements the Central Limit Order Book (CLOB) mechanics.
 */

#include "engine/order_book.hpp"

#include <iostream>
#include <iomanip>

namespace engine {

using namespace core;

OrderBook::OrderBook(const std::string& instrument)
    : instrument_(instrument) {}

/**
 * Add an order to the book and return any resulting trades.
 */
std::vector<Trade> OrderBook::addOrder(const Order& order) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (order.type == OrderType::MARKET || 
        (order.type == OrderType::LIMIT &&
        ((order.side == Side::BUY && !asks_.empty() && order.price >= asks_.begin()->first) ||
        (order.side == Side::SELL && !bids_.empty() && order.price <= bids_.begin()->first)))) {
        
        auto trades = match(order);

        if (trade_callback_) {
            for (const auto& t : trades) {
                trade_callback_(t);
            }
        }

        return trades;
    }

    if (order.type == OrderType::LIMIT) {
        insertLimitOrder(order);
        std::cout << "[OrderBook] Added " 
                  << (order.side == Side::BUY ? "BUY" : "SELL")
                  << " order ID " << order.id
                  << " @ " << order.price
                  << " x " << order.quantity << std::endl;
    }

    return {};
}

/**
 * Match an order against the opposing side of the book.
 */
std::vector<Trade> OrderBook::match(const Order& order) {
    std::vector<Trade> trades;

    if (order.side == Side::BUY) {
        // match against asks
        while (!asks_.empty() && order.quantity > 0) {
            auto price_level = asks_.begin();
            double match_price = price_level->first;
            auto& queue = price_level->second;

            while (!queue.empty() && order.quantity > 0) {
                Order resting = queue.front();
                uint32_t traded_qty = std::min(order.quantity, resting.quantity);

                Trade trade(
                    next_trade_id_++,
                    order.id,
                    resting.id,
                    instrument_,
                    match_price,
                    traded_qty,
                    order.timestamp,
                    Side::BUY
                );
                trades.push_back(trade);

                std::cout << "[OrderBook] Trade executed: "
                          << "Trade ID " << trade.trade_id
                          << ", Buy ID " << trade.buy_order_id
                          << ", Sell ID " << trade.sell_order_id
                          << ", Price " << trade.price
                          << ", Quantity " << trade.quantity << std::endl;

                // update or remove resting order
                if (traded_qty == resting.quantity) {
                    queue.pop_front();
                    orders_.erase(resting.id);
                } else {
                    queue.front().quantity -= traded_qty;
                }

                // reduce incoming order quantity
                const_cast<Order&>(order).quantity -= traded_qty;
            }

            if (queue.empty()) {
                asks_.erase(price_level);
            }
        }
    } else {
        // match against bids
        while (!bids_.empty() && order.quantity > 0) {
            auto price_level = bids_.begin();
            double match_price = price_level->first;
            auto& queue = price_level->second;

            while (!queue.empty() && order.quantity > 0) {
                Order resting = queue.front();
                uint32_t traded_qty = std::min(order.quantity, resting.quantity);

                Trade trade(
                    next_trade_id_++,
                    resting.id,
                    order.id,
                    instrument_,
                    match_price,
                    traded_qty,
                    order.timestamp,
                    Side::SELL
                );
                trades.push_back(trade);

                std::cout << "[OrderBook] Trade executed: "
                          << "Trade ID " << trade.trade_id
                          << ", Buy ID " << trade.buy_order_id
                          << ", Sell ID " << trade.sell_order_id
                          << ", Price " << trade.price
                          << ", Quantity " << trade.quantity << std::endl;

                // update or remove resting order
                if (traded_qty == resting.quantity) {
                    queue.pop_front();
                    orders_.erase(resting.id);
                } else {
                    queue.front().quantity -= traded_qty;
                }

                // reduce incoming order quantity (note: order is const so we can't modify it directly)
                const_cast<Order&>(order).quantity -= traded_qty;
            }

            if (queue.empty()) {
                bids_.erase(price_level);
            }
        }
    }

    return trades;
}

/**
 * Inserts a passive limit order into the book.
 */
void OrderBook::insertLimitOrder(const Order& order) {
    if (order.side == Side::BUY) {
        bids_[order.price].push_back(order);
    } else {
        asks_[order.price].push_back(order);
    }
    orders_[order.id] = order;
}

/**
 * Cancel a resting limit order by ID.
 */
bool OrderBook::cancelOrder(uint64_t order_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Search bids_
    for (auto bid_it = bids_.begin(); bid_it != bids_.end(); ++bid_it) {
        auto& queue = bid_it->second;
        for (auto q_it = queue.begin(); q_it != queue.end(); ++q_it) {
            if (q_it->id == order_id) {
                queue.erase(q_it);
                if (queue.empty()) bids_.erase(bid_it);
                orders_.erase(order_id);
                std::cout << "[OrderBook] Canceled order ID " << order_id << std::endl;
                return true;
            }
        }
    }

    // search asks
    for (auto ask_it = asks_.begin(); ask_it != asks_.end(); ++ask_it) {
        auto& queue = ask_it->second;
        for (auto q_it = queue.begin(); q_it != queue.end(); ++q_it) {
            if (q_it->id == order_id) {
                queue.erase(q_it);
                if (queue.empty()) asks_.erase(ask_it);
                orders_.erase(order_id);
                std::cout << "[OrderBook] Canceled order ID " << order_id << std::endl;
                return true;
            }
        }
    }

    std::cout << "[OrderBook] Failed to cancel order ID " << order_id << " (not found)" << std::endl;
    return false;
}

const std::unordered_map<uint64_t, core::Order>& OrderBook::getOrders() const {
    return orders_;
}


/**
 * Prints a snapshot of the order book to stdout.
 */
void OrderBook::printBook() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::cout << "Order Book [" << instrument_ << "]\n";

    std::cout << "  Asks:\n";
    for (const auto& [price, queue] : asks_) {
        std::cout << "    " << std::fixed << std::setprecision(2) << price << " × " << queue.size() << "\n";
    }

    std::cout << "  Bids:\n";
    for (const auto& [price, queue] : bids_) {
        std::cout << "    " << std::fixed << std::setprecision(2) << price << " × " << queue.size() << "\n";
    }
}

// Implementation for getBestBid and getBestAsk

std::optional<Order> OrderBook::getBestBid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bids_.empty()) return std::nullopt;
    const auto& queue = bids_.begin()->second;
    if (queue.empty()) return std::nullopt;
    return queue.front();
}

std::optional<Order> OrderBook::getBestAsk() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (asks_.empty()) return std::nullopt;
    const auto& queue = asks_.begin()->second;
    if (queue.empty()) return std::nullopt;
    return queue.front();
}

void OrderBook::setTradeCallback(std::function<void(const Trade&)> cb) {
    trade_callback_ = cb;
}

}