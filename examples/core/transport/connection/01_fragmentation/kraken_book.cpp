#include "run_example.hpp"

int main() {
    // Run the Kraken order book snapshot example (high fragmentation potential)
    return run_example(
        "Kraken",
        "wss://ws.kraken.com/v2",
        "Public BTC/USD full order book snapshot (Kraken WebSocket v2)",
        R"({
            "method": "subscribe",
            "params": {
                "channel": "book",
                "symbol": ["BTC/USD"],
                "depth": 1000,
                "snapshot": true
            }
        })"
    );
}