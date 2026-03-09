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

#include "wirekrak/core/preset/protocol/kraken_default.hpp"
#include "lcr/memory/block_pool.hpp"
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
// Setup environment
// -----------------------------------------------------------------------------
using namespace wirekrak::core;
using namespace wirekrak::core::protocol::kraken;

// -------------------------------------------------------------------------
// Global memory block pool
// -------------------------------------------------------------------------
inline constexpr static std::size_t BLOCK_SIZE = 128 * 1024; // 128 KiB
inline constexpr static std::size_t BLOCK_COUNT = 8;
static lcr::memory::block_pool memory_pool(BLOCK_SIZE, BLOCK_COUNT);

// -----------------------------------------------------------------------------
// Golbal SPSC ring buffer (transport → session)
// -----------------------------------------------------------------------------
static preset::DefaultMessageRing message_ring(memory_pool);


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

    auto symbols = to_request_symbols(params.symbols);

    // -------------------------------------------------------------------------
    // Session setup
    // -------------------------------------------------------------------------
    preset::protocol::kraken::DefaultSession session(message_ring);

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
        trade::Subscribe{ .symbols = symbols, .snapshot = params.snapshot }
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
    if (session.is_connected()) {
        (void)session.unsubscribe(
            trade::Unsubscribe{ .symbols = symbols }
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
