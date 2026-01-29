#include "run_example.hpp"

int main() {
    // Run the OKX ticker subscription example
    return run_example(
        "Bybit",
        "wss://stream.bybit.com/v5/public/spot",
        "Minimal connection lifecycle probe (Bybit WebSocket v5)"
    );
}
