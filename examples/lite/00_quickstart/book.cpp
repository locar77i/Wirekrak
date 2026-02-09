// ============================================================================
// Lite example 00_quickstart
//
// Demonstrates:
// - Connecting to Kraken WebSocket API
// - Subscribing to a single book stream
// - Consuming data via a callback
// - Clean shutdown via Ctrl+C
// ============================================================================
#include <iostream>
#include <chrono>
#include <csignal>
#include <atomic>
#include <thread>

// SDK v1 invariant:
// - Each callback corresponds to one price level update
// - snapshot delivers full depth
// - update delivers incremental changes
#include "wirekrak.hpp"

#include "common/logger.hpp"

// -----------------------------------------------------------------------------
// Ctrl+C handling
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}

int main() {
    using namespace wirekrak::lite;

    wirekrak::log::set_level("info");

    std::signal(SIGINT, on_signal);  // Handle Ctrl+C

    // -------------------------------------------------------------------------
    // Client setup
    // -------------------------------------------------------------------------
    Client client{"wss://ws.kraken.com/v2"};

    // This example focuses on the minimal client lifecycle.
    // Error handling and advanced hooks are demonstrated in later examples.

    if (!client.connect()) {
        std::cerr << "[wirekrak-lite] Failed to connect\n";
        return -1;
    }

    // -------------------------------------------------------------------------
    // Subscribe to BTC/EUR book updates
    // -------------------------------------------------------------------------
    int messages_received = 0;

    client.subscribe_book(
        {"BTC/EUR"},
        [&](const BookLevel& lvl) {
            std::cout << " -> " << lvl << std::endl;
            ++messages_received;
        },
        true  // request an initial snapshot before live updates (recommended)
    );

    // -------------------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // Stop after ~60 book messages or when the user interrupts (Ctrl+C)
    // -------------------------------------------------------------------------
    while (running.load(std::memory_order_relaxed) && messages_received < 60) {
        client.poll();   // Drives the client state machine and dispatches callbacks
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Unsubscribe & exit
    // -------------------------------------------------------------------------
    client.unsubscribe_book({"BTC/EUR"});

    std::cout << "\n[wirekrak-lite] Done.\n";
    return 0;
}
