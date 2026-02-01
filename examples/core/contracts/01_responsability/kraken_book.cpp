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
    Session session;

    // -------------------------------------------------------------------------
    // Control-plane observability
    // -------------------------------------------------------------------------
    session.on_status([](const schema::status::Update& status) {
        std::cout << " -> [STATUS] " << status << std::endl;
    });

    session.on_pong([](const schema::system::Pong& pong) {
        std::cout << " -> [PONG] " << pong << std::endl;
    });

    session.on_rejection([](const schema::rejection::Notice& notice) {
        std::cout << " -> [REJECTION] " << notice << std::endl;
    });

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
    session.subscribe(
        schema::book::Subscribe{
            .symbols  = params.symbols,
            .depth    = params.depth,
            .snapshot = params.snapshot
        },
        [](const schema::book::Response& book) {
            std::cout << " -> [BOOK] " << book << std::endl;
        }
    );

    // -------------------------------------------------------------------------
    // Poll-driven execution loop
    // -------------------------------------------------------------------------
    while (running.load()) {
        session.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Explicit unsubscription
    // -------------------------------------------------------------------------
    session.unsubscribe(
        schema::book::Unsubscribe{
            .symbols = params.symbols,
            .depth   = params.depth
        }
    );

    // Drain a bounded window to process final ACKs
    auto drain_until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < drain_until) {
        session.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n[SUCCESS] Clean shutdown completed.\n";
    return 0;
}
