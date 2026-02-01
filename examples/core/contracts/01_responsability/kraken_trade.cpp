// ============================================================================
// Core Contracts Example — Protocol-Level Trade Subscription
//
// This example demonstrates how Wirekrak Core handles protocol-level
// subscriptions with explicit ACK tracking and observable control-plane events.
//
// CONTRACTS DEMONSTRATED:
//
// - Subscriptions are explicit protocol requests
// - ACKs are tracked internally by Core
// - Data-plane callbacks are routed deterministically
// - Control-plane events (status, pong, rejection) are independent
// - poll() is the only execution driver
//
// This is NOT a convenience wrapper.
// This example exposes the true Core interaction model.
// ============================================================================

#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>

#include "wirekrak/core.hpp"
#include "common/cli/trade.hpp"

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
    // Runtime configuration (no hard-coded behavior)
    // -------------------------------------------------------------------------
    const auto& params =
        wirekrak::cli::trade::configure(argc, argv,
            "Wirekrak Core — Protocol Trade Subscription Example\n"
            "Demonstrates explicit protocol subscriptions and ACK handling.\n",
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
    // Explicit protocol subscription
    // -------------------------------------------------------------------------
    session.subscribe(
        schema::trade::Subscribe{
            .symbols  = params.symbols,
            .snapshot = params.snapshot
        },
        [](const schema::trade::ResponseView& trade) {
            std::cout << " -> [TRADE] " << trade << std::endl;
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
        schema::trade::Unsubscribe{ .symbols = params.symbols }
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
