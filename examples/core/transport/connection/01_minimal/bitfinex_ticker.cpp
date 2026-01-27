#include "run_example.hpp"

int main() {
    // Run the Bitfinex ticker subscription example
    return run_example(
        "Bitfinex",
        "wss://api-pub.bitfinex.com/ws/2",
        "Public BTCUSD ticker feed (Bitfinex WebSocket v2)",
        R"({
            "event": "subscribe",
            "channel": "ticker",
            "symbol": "tBTCUSD"
        })"
    );
}
