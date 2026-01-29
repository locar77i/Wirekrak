#include "run_example.hpp"

int main() {
    // Run the Bitstamp ticker subscription example
    return run_example(
        "Bitstamp",
        "wss://ws.bitstamp.net/s/v2/",
        "Minimal connection lifecycle probe (Bitstamp WebSocket)"
    );
}
