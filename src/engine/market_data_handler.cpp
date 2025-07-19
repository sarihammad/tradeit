/**
 * @file market_data_handler.cpp
 * @brief Implements MarketDataHandler for reading and feeding tick data.
 */

#include "engine/market_data_handler.hpp"

#include <sstream>
#include <chrono>
#include <thread>
#include <iostream>

namespace engine {

using namespace core;

MarketDataHandler::MarketDataHandler(const std::string& file_path)
    : file_path_(file_path), running_(false), callback_(nullptr) {}

void MarketDataHandler::setOrderCallback(OrderCallback cb) {
    this->callback_ = cb;
}

void MarketDataHandler::start(OrderCallback callback) {
    running_ = true;
    worker_ = std::thread(&MarketDataHandler::feedLoop, this, callback);
}

void MarketDataHandler::stop() {
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
}

void MarketDataHandler::feedLoop(OrderCallback callback) {
    std::ifstream infile(file_path_);
    if (!infile.is_open()) {
        std::cerr << "Failed to open market data file: " << file_path_ << "\n";
        return;
    }

    std::string line;
    // Skip header line if present
    std::getline(infile, line);
    if (line.find("timestamp") != std::string::npos) {
        // It was a header, skip it
    } else {
        // Not a header, rewind to start
        infile.clear();
        infile.seekg(0);
    }

    while (running_ && std::getline(infile, line)) {
        if (line.empty()) continue;

        std::vector<std::string> fields;
        std::istringstream ss(line);
        std::string token;
        while (std::getline(ss, token, ',')) {
            fields.push_back(token);
        }

        if (fields.size() != 6) {
            std::cerr << "Skipping malformed line: " << line << "\n";
            continue;
        }

        try {
            Order order = parseLine(line);
            if (callback_) callback_(order);
            std::cout << "[MarketDataHandler] Order parsed: "
                      << order.instrument << " "
                      << (order.side == Side::BUY ? "BUY" : "SELL")
                      << " @ " << order.price
                      << " x " << order.quantity
                      << ", Time: " << order.timestamp << std::endl;
        } catch (const std::exception& ex) {
            std::cerr << "Failed to parse line: " << ex.what() << "\n";
        }

        // Optional: simulate time delay between ticks
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[MarketDataHandler] Finished processing market data file." << std::endl;

    infile.close();
}

void MarketDataHandler::load() {
    std::ifstream infile(file_path_);
    if (!infile.is_open()) {
        throw std::runtime_error("Failed to open market data file: " + file_path_);
    }

    std::string line;
    std::getline(infile, line); // skip header
    if (line.find("timestamp") == std::string::npos) {
        infile.clear();
        infile.seekg(0);
    }

    while (std::getline(infile, line)) {
        if (line.empty()) continue;

        std::vector<std::string> fields;
        std::istringstream ss(line);
        std::string token;
        while (std::getline(ss, token, ',')) {
            fields.push_back(token);
        }

        if (fields.size() != 6) {
            std::cerr << "Load error: Invalid field count\n";
            continue;
        }

        try {
            Order order = parseLine(line);
            if (callback_) callback_(order);
        } catch (const std::exception& ex) {
            std::cerr << "Load error: " << ex.what() << "\n";
        }
    }

    infile.close();
}

Order MarketDataHandler::parseLine(const std::string& line) {
    std::istringstream ss(line);
    std::string token;

    std::vector<std::string> fields;
    while (std::getline(ss, token, ',')) {
        fields.push_back(token);
    }

    if (fields.size() != 6) {
        throw std::runtime_error("Invalid field count");
    }

    uint64_t timestamp = std::stoull(fields[0]);
    std::string symbol = fields[1];
    std::string side_str = fields[2];
    double price = std::stod(fields[3]);
    uint32_t quantity = std::stoul(fields[4]);
    std::string type_str = fields[5];

    Side side = (side_str == "BUY") ? Side::BUY : Side::SELL;
    OrderType type = (type_str == "LIMIT") ? OrderType::LIMIT : OrderType::MARKET;

    return Order(core::Order::global_order_id++, symbol, type, side, price, quantity, timestamp);
}

}