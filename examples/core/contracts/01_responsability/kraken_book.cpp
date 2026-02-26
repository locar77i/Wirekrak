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
#include "common/loop/helpers.hpp"

// -----------------------------------------------------------------------------
// Lifecycle control
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}

// -----------------------------------------------------------------------------
// Setup environment
// -----------------------------------------------------------------------------
using namespace wirekrak::core;
using namespace wirekrak::core::protocol::kraken;

static MessageRingT g_ring;   // Golbal SPSC ring buffer (transport → session)


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    using namespace schema;

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
    SessionT session(g_ring);

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
    int idle_spins = 0;
    bool did_work = false;
    while (running.load(std::memory_order_relaxed) && session.is_active()) {
        (void)session.poll();
        did_work = loop::drain_and_print_messages(session);
        // Yield to avoid busy-waiting when idle
        loop::manage_idle_spins(did_work, idle_spins);
    }

    // -------------------------------------------------------------------------
    // Explicit unsubscription
    // -------------------------------------------------------------------------
    if (session.is_active()) {
        (void)session.unsubscribe(
            book::Unsubscribe{ .symbols = params.symbols, .depth = params.depth }
        );
    }

    // -------------------------------------------------------------------------
    // Graceful shutdown: drain until protocol is idle and close session
    // -------------------------------------------------------------------------
    while (!session.is_idle()) {
        (void)session.poll();
        loop::drain_messages(session);
        std::this_thread::yield();
    }

    session.close();

    // -------------------------------------------------------------------------
    // Dump telemetry
    // -------------------------------------------------------------------------
    session.telemetry().debug_dump(std::cout);

    std::cout << "\n[SUCCESS] Clean shutdown completed.\n";
    return 0;
}
