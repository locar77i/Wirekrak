#include "run_example.hpp"

int main() {
    // Run the Bitfinex full order book snapshot example (high fragmentation potential)
    return run_example(
        "Bitfinex",
        "wss://api-pub.bitfinex.com/ws/2",
        "Public BTC/USD full order book snapshot (Bitfinex WebSocket v2)",
        R"({
            "event": "subscribe",
            "channel": "book",
            "symbol": "tBTCUSD",
            "prec": "P0",
            "freq": "F0",
            "len": 250
        })"
    );
}
