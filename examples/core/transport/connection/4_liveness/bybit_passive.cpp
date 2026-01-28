#include "run_example.hpp"

int main() {
    return run_example(
        "Bybit",
        "wss://stream.bybit.com/v5/public/spot",
        "Passive probe with protocol-managed ping (Bybit WebSocket v5)",
        R"({"op":"ping"})",
        2   // enable ping after 2 reconnects
    );
}
