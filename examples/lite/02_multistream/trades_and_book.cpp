// ============================================================================
// Lite example 02_multistream
//
// Demonstrates:
// - Consuming multiple market data streams using a single client
// - Independent callbacks for different stream types
// - A shared polling loop and lifecycle
//
// No threading, aggregation, or protocol logic is introduced.
// ============================================================================

#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>

// SDK v1 invariants:
// - Trade callbacks are ordered per symbol
// - Book callbacks represent individual price level updates
// - Snapshot messages (when requested) precede live updates
#include "wirekrak.hpp"

#include "common/logger.hpp"

// -----------------------------------------------------------------------------
// Ctrl+C handling
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    using namespace wirekrak::lite;

    wirekrak::log::set_level("info");

    // -------------------------------------------------------------
    // Signal handling
    // -------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------
    // Client setup
    // -------------------------------------------------------------
    Client client{"wss://ws.kraken.com/v2"};

    // Note:
    // - Error and lifecycle hooks are available (e.g. `on_error()`),
    //   but intentionally omitted here to keep this example focused.
    // - See `01_subscriptions` for configurable handling.

    if (!client.connect()) {
        std::cerr << "[wirekrak-lite] Failed to connect\n";
        return -1;
    }

    // -------------------------------------------------------------
    // Trade subscription
    // -------------------------------------------------------------
    client.subscribe_trades(
        {"BTC/EUR"},
        [](const Trade& t) {
            std::cout << " -> " << t << std::endl;
        },
        true  // request an initial snapshot before live updates
    );

    // -------------------------------------------------------------
    // Book subscription
    // -------------------------------------------------------------
    client.subscribe_book(
        {"BTC/EUR"},
        [](const BookLevel& lvl) {
            std::cout << " -> " << lvl << std::endl;
        },
        true  // request an initial snapshot before live updates
    );

    // -------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // -------------------------------------------------------------
    while (running.load(std::memory_order_relaxed)) {
        client.poll();   // Drives the client state machine and dispatches callbacks
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------
    // Unsubscribe & shutdown
    // -------------------------------------------------------------
    client.unsubscribe_trades({"BTC/EUR"});
    client.unsubscribe_book({"BTC/EUR"});

    // Drain events before exit to allow in-flight messages
    // to be delivered and callbacks to complete
    for (int i = 0; i < 200; ++i) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n[wirekrak-lite] Done.\n";
    return 0;
}
