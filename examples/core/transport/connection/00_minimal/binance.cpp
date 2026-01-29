#include "run_example.hpp"

int main() {
    return run_example(
        "Binance",
        "wss://stream.binance.com:9443/ws",
        "Minimal connection lifecycle probe (Binance WebSocket)"
    );
}