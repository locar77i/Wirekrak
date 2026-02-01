// ============================================================================
// Lite example 01_subscriptions
//
// Demonstrates:
// - Configurable book subscriptions via CLI
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
// - Each callback corresponds to one price level update
// - snapshot delivers full depth
// - update delivers incremental changes
#include "wirekrak.hpp"

#include "common/cli/book.hpp"


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
    const auto& params = wirekrak::cli::book::configure(argc, argv,
        "Wirekrak Lite - Kraken Book Subscription Example\n"
        "This example let's you subscribe to book events on a given symbol from Kraken WebSocket API v2.\n",
        "This example runs indefinitely until interrupted.\n"
        "Press Ctrl+C to unsubscribe and exit cleanly.\n"
        "Let's enjoy trading with Wirekrak!"
    );
    params.dump("=== Book Example Parameters ===", std::cout);

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
    // Subscribe to book updates
    // -------------------------------------------------------------
    auto book_handler = [](const BookLevel& lvl) {
        std::cout << " -> " << lvl << std::endl;
    };

    client.subscribe_book(params.symbols, book_handler, params.snapshot);

    // -------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // -------------------------------------------------------------
    while (running.load()) {
        client.poll();  // Drives the client state machine and dispatches callbacks
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------
    // Unsubscribe from book updates
    // -------------------------------------------------------------
    client.unsubscribe_book(params.symbols);

    // Drain events before exit to allow in-flight messages
    // to be delivered and callbacks to complete
    for (int i = 0; i < 200; ++i) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n[wirekrak-lite] Done.\n";
    return 0;
}
