# TradeIt

A high-performance multithreaded C++ simulator for algorithmic trading.

### Features

- Central Limit Order Book (CLOB)
- Multiple strategy execution using market making, momentum, arbitrage
- Concurrency with `std::thread`, `std::mutex`, and condition variables
- Real-time trade logging and risk monitoring
- Clean architecture and modular design

### How to Run

```bash
mkdir build && cd build
cmake ..
make
