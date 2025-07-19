/**
 * @file simulator.cpp
 * @brief Implements the simulated exchange driver that processes orders and emits trades.
 */

#include "engine/simulator.hpp"

namespace engine {

using namespace core;
using namespace strategy;

void Simulator::registerStrategy(std::shared_ptr<Strategy> strategy) {
    std::lock_guard<std::mutex> lock(mutex_);
    strategies_.emplace_back(std::move(strategy));
}

void Simulator::onOrder(const Order& order) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& book = books_.try_emplace(order.instrument, order.instrument).first->second;
    auto trades = book.addOrder(order);

    for (const auto& trade : trades) {
        for (const auto& strategy : strategies_) {
            strategy->onTrade(trade);
        }
    }
}

void Simulator::start() {
    for (auto& strategy : strategies_) {
        strategy->start();
    }
}

void Simulator::stop() {
    for (auto& strategy : strategies_) {
        strategy->stop();
    }
}

}