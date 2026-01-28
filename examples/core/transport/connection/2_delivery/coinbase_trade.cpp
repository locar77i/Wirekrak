#include "run_example.hpp"

int main() {
    // Run the Coinbase trades subscription example
    return run_example(
        "Coinbase",
        "wss://ws-feed.exchange.coinbase.com",
        "Public BTC-USD trades feed (Coinbase WebSocket)",
        R"({
            "type": "subscribe",
            "channels": [
                {
                    "name": "matches",
                    "product_ids": ["BTC-USD"]
                }
            ]
        })"
    );
}
