#include "run_example.hpp"

int main() {
    // Run the Kraken ticker subscription example
    return run_example(
        "Kraken",
        "wss://ws.kraken.com/v2",
        "Public BTC/USD ticker feed (Kraken WebSocket v2)",
        R"({
            "method": "subscribe",
            "params": {
                "channel": "ticker",
                "symbol": ["BTC/USD"]
            }
        })"
    );
}
