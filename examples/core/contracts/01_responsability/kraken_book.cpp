// ============================================================================
// Core Contracts Example — Protocol-Level Book Subscription
//
// This example demonstrates how Wirekrak Core handles stateful, parameterized
// order book subscriptions with explicit ACK tracking.
//
// CONTRACTS DEMONSTRATED:
//
// - Book subscriptions are explicit protocol contracts (symbols, depth, snapshot)
// - Subscription parameters are enforced, not inferred
// - Data-plane callbacks are routed deterministically
// - Control-plane events (status, pong, rejection) are independent
// - poll() is the sole execution driver
//
// This example exposes the true Core interaction model for order book data.
//
// NOTE:
// Wirekrak Core exposes control-plane signals (status, pong, rejection)
// as pull-based state. This example demonstrates explicit observation
// without callbacks or reentrancy.
// ============================================================================

#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>

#include "wirekrak/core.hpp"
#include "common/cli/book.hpp"

// -----------------------------------------------------------------------------
// Lifecycle control
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}


// -----------------------------------------------------------------------------
// Helper to drain all available messages
// -----------------------------------------------------------------------------
inline void drain_messages(wirekrak::core::kraken::Session& session) {
    using namespace wirekrak::core::protocol::kraken::schema;

    // Observe latest pong (liveness signal)
    static system::Pong last_pong;
    if (session.try_load_pong(last_pong)) {
        std::cout << " -> " << last_pong << std::endl;
    }

    // Observe latest status
    static status::Update last_status;
    if (session.try_load_status(last_status)) {
        std::cout << " -> " << last_status << std::endl;
    }

    // Drain protocol errors (required)
    session.drain_rejection_messages([](const rejection::Notice& msg) {
        std::cout << " -> " << msg << std::endl;
    });

    // Drain data-plane messages (required)
    session.drain_book_messages([](const book::Response& msg) {
        std::cout << " -> " << msg << std::endl;
    });
}


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    using namespace wirekrak::core;
    using namespace protocol::kraken::schema;

    // -------------------------------------------------------------------------
    // Signal handling (explicit lifecycle control)
    // -------------------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------------------
    // Runtime configuration (symbols, depth, snapshot)
    // -------------------------------------------------------------------------
    const auto& params =
        wirekrak::cli::book::configure(argc, argv,
            "Wirekrak Core — Protocol Book Subscription Example\n"
            "Demonstrates explicit, stateful book subscriptions with ACK tracking.\n",
            "This example runs indefinitely until interrupted.\n"
            "Press Ctrl+C to unsubscribe and exit cleanly.\n"
            "Let's enjoy trading with Wirekrak!"
        );

    params.dump("=== Runtime Parameters ===", std::cout);

    // -------------------------------------------------------------------------
    // Session setup
    // -------------------------------------------------------------------------
    kraken::Session session;

    // -------------------------------------------------------------------------
    // Connect
    // -------------------------------------------------------------------------
    if (!session.connect(params.url)) {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Explicit protocol subscription (stateful)
    // -------------------------------------------------------------------------
    (void)session.subscribe(
        book::Subscribe{ .symbols = params.symbols, .depth = params.depth, .snapshot = params.snapshot }
    );

    // -------------------------------------------------------------------------
    // Poll-driven execution loop
    // -------------------------------------------------------------------------
    while (running.load(std::memory_order_relaxed)) {
        (void)session.poll();
        drain_messages(session);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Explicit unsubscription
    // -------------------------------------------------------------------------
    (void)session.unsubscribe(
        book::Unsubscribe{ .symbols = params.symbols, .depth = params.depth }
    );

    // -------------------------------------------------------------------------
    // Graceful shutdown: drain until protocol is idle and close session
    // -------------------------------------------------------------------------
    while (!session.is_idle()) {
        (void)session.poll();
        drain_messages(session);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    session.close();

    std::cout << "\n[SUCCESS] Clean shutdown completed.\n";
    return 0;
}
