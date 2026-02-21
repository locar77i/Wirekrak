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
#include "common/cli/trade.hpp"
#include "common/loop/helpers.hpp"

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
    using namespace protocol::kraken::schema;

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
    kraken::Session session;

    // -------------------------------------------------------------------------
    // Connect
    // -------------------------------------------------------------------------
    if (!session.connect(params.url)) {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Explicit protocol subscription
    // -------------------------------------------------------------------------
    (void)session.subscribe(
        trade::Subscribe{ .symbols = params.symbols, .snapshot = params.snapshot }
    );

    // -------------------------------------------------------------------------
    // Poll-driven execution loop
    // -------------------------------------------------------------------------
    int idle_spins = 0;
    bool did_work = false;
    while (running.load(std::memory_order_relaxed)) {
        (void)session.poll();
        did_work = loop::drain_messages(session);
        // Yield to avoid busy-waiting when idle
        loop::manage_idle_spins(did_work, idle_spins);
    }

    // -------------------------------------------------------------------------
    // Explicit unsubscription
    // -------------------------------------------------------------------------
    (void)session.unsubscribe(
        trade::Unsubscribe{ .symbols = params.symbols }
    );

    // -------------------------------------------------------------------------
    // Graceful shutdown: drain until protocol is idle and close session
    // -------------------------------------------------------------------------
    while (!session.is_idle()) {
        (void)session.poll();
        loop::drain_messages(session);
        std::this_thread::yield();
    }

    session.close();

    std::cout << "\n[SUCCESS] Clean shutdown completed.\n";
    return 0;
}
