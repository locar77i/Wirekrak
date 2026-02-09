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
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    using namespace wirekrak::core;
    namespace schema = protocol::kraken::schema;

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
        std::cerr << "Failed to connect\n";
        return -1;
    }

    // -------------------------------------------------------------------------
    // Explicit protocol subscription (stateful)
    // -------------------------------------------------------------------------
    (void)session.subscribe(
        schema::book::Subscribe{ .symbols = params.symbols, .depth = params.depth, .snapshot = params.snapshot }
    );

    // -------------------------------------------------------------------------
    // Poll-driven execution loop
    // -------------------------------------------------------------------------
    schema::system::Pong last_pong;
    schema::status::Update last_status;
    schema::rejection::Notice rejection;
    schema::book::Response book_msg;
    while (running.load()) {
        (void)session.poll();
        // --- Observe latest pong (liveness signal) ---
        if (session.try_load_pong(last_pong)) {
            std::cout << " -> " << last_pong << std::endl;
        }
        // --- Observe latest status ---
        if (session.try_load_status(last_status)) {
            std::cout << " -> " << last_status << std::endl;
        }
        // Drain rejection messages in a loop until empty, to ensure we process all rejections received in this poll
        session.drain_rejection_messages([&](const schema::rejection::Notice& msg) {
            std::cout << " -> " << msg << std::endl;
        });
        // Drain book messages in a loop until empty, to ensure we process all messages received in this poll
        session.drain_book_messages([&](const schema::book::Response& msg) {
            std::cout << " ->" << msg << std::endl;
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Explicit unsubscription
    // -------------------------------------------------------------------------
    (void)session.unsubscribe(
        schema::book::Unsubscribe{ .symbols = params.symbols, .depth = params.depth
        }
    );

    // Drain a bounded window to process final ACKs
    auto drain_until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < drain_until) {
        (void)session.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n[SUCCESS] Clean shutdown completed.\n";
    return 0;
}
