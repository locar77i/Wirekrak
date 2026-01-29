#include "run_example.hpp"

int main() {
    // Run the Binance trades subscription example
    return run_example(
        "Binance",
        "wss://stream.binance.com:9443/ws/btcusdt@trade",
        "Public BTCUSDT trades feed (Binance WebSocket)",
        nullptr,
        std::chrono::seconds(10)
    );
}