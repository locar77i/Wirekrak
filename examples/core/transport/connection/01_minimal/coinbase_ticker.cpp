#include "run_example.hpp"

int main() {
    // Run the Coinbase ticker subscription example
    return run_example(
        "Coinbase",
        "wss://advanced-trade-ws.coinbase.com",
        "Public BTC-USD ticker feed (Coinbase Advanced Trade WebSocket)",
        R"({
            "type": "subscribe",
            "channel": "ticker",
            "product_ids": ["BTC-USD"]
        })"
    );
}
