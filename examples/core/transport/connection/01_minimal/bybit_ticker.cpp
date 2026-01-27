#include "run_example.hpp"

int main() {
    // Run the OKX ticker subscription example
    return run_example(
        "Bybit",
        "wss://stream.bybit.com/v5/public/spot",
        "Public BTCUSDT ticker feed (Bybit WebSocket v5)",
        R"({
            "op": "subscribe",
            "req_id": "unique_id_12345",
            "args": [
                "tickers.BTCUSDT"
            ]
        })"
    );
}
