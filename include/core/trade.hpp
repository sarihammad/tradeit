/**
 * @file trade.hpp
 * @brief Defines the Trade structure to represent executed orders.
 */

#pragma once

#include <string>
#include <cstdint>

namespace core {

/**
 * @struct Trade
 * @brief Represents a completed trade between two orders.
 *
 * Created when a buy and sell order are matched by the order book.
 */
struct Trade {
    uint64_t trade_id;        ///< Unique trade identifier
    uint64_t buy_order_id;    ///< ID of the buy-side order
    uint64_t sell_order_id;   ///< ID of the sell-side order
    std::string instrument;   ///< Symbol of traded instrument
    double price;             ///< Executed price per unit
    uint32_t quantity;        ///< Number of units traded
    uint64_t timestamp;       ///< Execution timestamp (μs)
    core::Side side;          ///< Direction of the trade (BUY or SELL)

    /**
     * @brief Constructs a new Trade instance.
     *
     * @param tid       Trade ID
     * @param buy_id    Buy order ID
     * @param sell_id   Sell order ID
     * @param instr     Instrument symbol
     * @param p         Executed price
     * @param q         Quantity traded
     * @param ts        Timestamp (microseconds)
     * @param s         Direction of the trade (BUY or SELL)
     */
    Trade(uint64_t tid,
          uint64_t buy_id,
          uint64_t sell_id,
          const std::string& instr,
          double p,
          uint32_t q,
          uint64_t ts,
          core::Side s)
        : trade_id(tid),
          buy_order_id(buy_id),
          sell_order_id(sell_id),
          instrument(instr),
          price(p),
          quantity(q),
          timestamp(ts),
          side(s) {}
};

}