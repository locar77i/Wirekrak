#include "run_example.hpp"

int main() {
    // Run the Kraken ticker subscription example
    return run_example(
        "Kraken",
        "wss://ws.kraken.com/v2",
        "Minimal connection lifecycle probe (Kraken WebSocket v2)"
    );
}
