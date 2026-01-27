#include "run_example.hpp"

int main() {
    return run_example(
        "Coinbase",
        "wss://ws-feed.exchange.coinbase.com",
        "Passive probe with protocol-managed ping (Coinbase WebSocket)",
        R"({"type":"ping"})",
        2   // enable ping after 2 reconnects
    );
}
