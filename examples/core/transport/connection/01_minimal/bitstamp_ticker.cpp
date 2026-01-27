#include "run_example.hpp"

int main() {
    // Run the Bitstamp ticker subscription example
    return run_example(
        "Bitstamp",
        "wss://ws.bitstamp.net/s/v2/",
        "Public BTCUSD ticker feed (Bitstamp WebSocket)",
        R"({
            "event": "bts:subscribe",
            "data": {
                "channel": "live_trades_btcusd"
            }
        })"
    );
}
