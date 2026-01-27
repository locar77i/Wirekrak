#include "run_example.hpp"

int main() {
    return run_example(
        "Kraken",
        "wss://ws.kraken.com/v2",
        "Passive probe with protocol-managed ping (Kraken WebSocket v2)",
        R"({"method":"ping"})",
        2   // enable ping after 2 reconnects
    );
}
