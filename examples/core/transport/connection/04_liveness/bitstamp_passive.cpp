#include "run_example.hpp"

int main() {
    return run_example(
        "Bitstamp",
        "wss://ws.bitstamp.net",
        "Passive probe with protocol-managed ping (Bitstamp WebSocket)",
        R"({"event":"bts:ping"})",
        2   // enable ping after 2 reconnects
    );
}
