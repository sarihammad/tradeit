/**
 * @file order.cpp
 * @brief Implements global order ID for unique order tracking.
 */

#include "core/order.hpp"

namespace core {

// initialize the global atomic order ID counter
std::atomic<uint64_t> Order::global_order_id{1};

}