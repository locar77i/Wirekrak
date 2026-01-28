#include "run_example.hpp"

int main() {
    return run_example(
        "Bitfinex",
        "wss://api-pub.bitfinex.com/ws/2",
        "Passive probe with protocol-managed ping (Bitfinex WebSocket v2)",
        R"({"event":"ping"})",
        2   // enable ping after 2 reconnects
    );
}