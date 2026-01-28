#include "run_example.hpp"

int main() {
    // Run the Bitfinex ticker subscription example
    return run_example(
        "Bitfinex",
        "wss://api-pub.bitfinex.com/ws/2",
        "Minimal connection lifecycle probe (Bitfinex WebSocket v2)"
    );
}
