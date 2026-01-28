#include "run_example.hpp"

int main() {
    // Run the Coinbase ticker subscription example
    return run_example(
        "Coinbase",
        "wss://advanced-trade-ws.coinbase.com",
        "Minimal connection lifecycle probe (Coinbase Advanced Trade WebSocket)"
    );
}
