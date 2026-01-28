#include "run_example.hpp"

int main() {
    // Run the Kraken trades subscription example
    return run_example(
        "Kraken",
        "wss://ws.kraken.com/v2",
        "Public BTC/USD trades feed (Kraken WebSocket v2)",
        R"({
            "method": "subscribe",
            "params": {
                "channel": "trade",
                "symbol": ["BTC/USD"]
            }
        })"
    );
}
