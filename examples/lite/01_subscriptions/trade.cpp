// ============================================================================
// Lite example 01_subscriptions
//
// Demonstrates:
// - Configurable trade subscriptions via CLI
// - Subscribing to multiple symbols
// - Error handling callbacks
// - Clean unsubscribe and shutdown
// ============================================================================
#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>

// SDK v1 invariant:
// - Each callback corresponds to exactly one trade
// - tag indicates snapshot vs live update
// - ordering is preserved per symbol
#include "wirekrak.hpp"

#include "common/cli/trade.hpp"


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
int main(int argc, char** argv) {
    using namespace wirekrak::lite;

    // -------------------------------------------------------------
    // Signal handling
    // -------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------
    // CLI parsing
    // -------------------------------------------------------------
    const auto& params = wirekrak::cli::trade::configure(argc, argv,
        "Wirekrak Lite - Kraken Trade Subscription Example\n"
        "This example let's you subscribe to trade events on a given symbol from Kraken WebSocket API v2.\n",
        "This example runs indefinitely until interrupted.\n"
        "Press Ctrl+C to unsubscribe and exit cleanly.\n"
        "Let's enjoy trading with Wirekrak!"
    );
    params.dump("=== Trade Example Parameters ===", std::cout);

    // -------------------------------------------------------------
    // Client setup
    // -------------------------------------------------------------
    Client client{params.url};

    // Error handling is configurable via callbacks.
    // Other lifecycle hooks are demonstrated in later examples.
    client.on_error([](const Error& err) {
        std::cerr << "[wirekrak-lite] error: " << err.message << "\n";
    });

    if (!client.connect()) {
        std::cerr << "[wirekrak-lite] Failed to connect\n";
        return -1;
    }

    // -------------------------------------------------------------
    // Trade subscription
    // -------------------------------------------------------------
    auto trade_handler = [](const Trade& t) {
        std::cout << " -> " << t << std::endl;
    };

    client.subscribe_trades(params.symbols, trade_handler, params.snapshot);

    // -------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // -------------------------------------------------------------
    client.run_while([&]() {
        return running.load(std::memory_order_relaxed);
    });

    // -------------------------------------------------------------------------
    // Unsubscribe & graceful shutdown
    // -------------------------------------------------------------------------
    client.unsubscribe_trades(params.symbols);
    client.run_until_idle();  // Ensure all protocol work and callbacks are complete before exiting
    client.disconnect();

    std::cout << "\n[wirekrak-lite] Done.\n";
    return 0;
}
