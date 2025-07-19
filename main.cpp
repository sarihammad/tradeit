#include "core/order.hpp"
#include "engine/order_book.hpp"
#include "engine/market_data_handler.hpp"
#include "engine/simulator.hpp"
#include "strategy/market_maker.hpp"
#include "strategy/momentum_trader.hpp"
#include "strategy/arbitrage_trader.hpp"
#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <csignal>
#include <atomic>
#include <unordered_map>

using json = nlohmann::json;

using namespace core;
using namespace engine;
using namespace strategy;

// Global stop signal
std::atomic<bool> running{true};

// Graceful shutdown on Ctrl+C
void signalHandler(int signum) {
    std::cout << "\n[INFO] Interrupt signal received. Shutting down...\n";
    running = false;
}

int main(int argc, char* argv[]) {
    json config;
    std::ifstream cfg("config.json");
    cfg >> config;

    signal(SIGINT, signalHandler);

    std::unordered_map<std::string, std::string> args;
    for (int i = 1; i < argc - 1; ++i) {
        std::string key = argv[i];
        if (key.rfind("--", 0) == 0 && (i + 1 < argc)) {
            args[key.substr(2)] = argv[i + 1];
            ++i;
        }
    }

    std::string strategy = args.count("strategy") ? args["strategy"] : static_cast<std::string>(config["strategy"]);
    std::string file = args.count("file") ? args["file"] : static_cast<std::string>(config["file"]);
    double spread = args.count("spread") ? std::stod(args["spread"]) : config.value("spread", 0.02);
    int size = args.count("size") ? std::stoi(args["size"]) : config.value("size", 10);
    double max_loss = args.count("risk") ? std::stod(args["risk"]) : config.value("risk", -500.0);

    std::cout << "[ENGINE] Strategy: " << strategy
              << ", File: " << file
              << ", Spread: " << spread
              << ", Size: " << size
              << ", Max Loss: " << max_loss << "\n";

    Simulator simulator;

    std::shared_ptr<Strategy> strat;

    static engine::OrderBook shared_book("ETH-USD");
    if (strategy == "marketmaker") {
        strat = std::make_shared<MarketMaker>("ETH-USD", shared_book,
            [&](const Order& o) { simulator.onOrder(o); }, max_loss);
    } else if (strategy == "momentum") {
        strat = std::make_shared<MomentumTrader>("ETH-USD",
            [&](const Order& o) { simulator.onOrder(o); }, max_loss);
    } else if (strategy == "arbitrage") {
        strat = std::make_shared<ArbitrageTrader>("ETH-USD", "BTC-USD",
            [&](const Order& o) { simulator.onOrder(o); }, spread, size, max_loss);
    } else {
        std::cerr << "[ERROR] Unknown strategy: " << strategy << std::endl;
        return 1;
    }

    simulator.registerStrategy(strat);
    simulator.start();

    MarketDataHandler md_handler(file);
    md_handler.start([&](const Order& o) {
        simulator.onOrder(o);
        strat->onMarketData(o);
    });

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    md_handler.stop();
    simulator.stop();

    strat->printSummary();
    strat->exportSummary("logs/summary.json");

    std::cout << "[ENGINE] Shutdown complete.\n";
    return 0;
}