#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

// SDK v1 invariant:
// - Each callback corresponds to exactly one trade
// - tag indicates snapshot vs live update
// - ordering is preserved per symbol
#include "wirekrak.hpp"
using namespace wirekrak::lite;


// -----------------------------------------------------------------------------
// Ctrl+C handling
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}


int main() {
    std::signal(SIGINT, on_signal);  // Handle Ctrl+C

    // -------------------------------------------------------------------------
    // Client setup
    // -------------------------------------------------------------------------
    Client client{"wss://ws.kraken.com/v2"};   // 1) Create client and connect to Kraken WebSocket API v2
    if (!client.connect()) {
        std::cerr << "[wirekrak-lite] Failed to connect\n";
        return -1;
    }

    // -------------------------------------------------------------------------
    // Subscribe to BTC/EUR trade updates
    // -------------------------------------------------------------------------
    int messages_received = 0;   // 2) Subscribe to BTC/EUR trades
    client.subscribe_trades({"BTC/EUR"},
                     [&](const Trade& t) {
                            std::cout << " -> " << t << std::endl;
                            ++messages_received;
                     },
                     true   // snapshot
    );

    // -------------------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // -------------------------------------------------------------------------
    while (running.load(std::memory_order_relaxed) && messages_received < 60) {
        client.poll();           // REQUIRED to process incoming messages
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Unsubscribe & exit
    // -------------------------------------------------------------------------
    client.unsubscribe_trades({"BTC/EUR"});   // 3) Unsubscribe from BTC/EUR trades
    
    std::cout << "\n[wirekrak-lite] Done.\n";
    return 0;
}
