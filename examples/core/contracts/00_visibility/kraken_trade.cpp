// ============================================================================
// Core Contracts Example — Minimal Poll-Driven Execution
//
// This example demonstrates the most fundamental Wirekrak Core contract:
//
//   ➜ Nothing happens unless poll() is called.
//
// CONTRACTS DEMONSTRATED:
//
// - Core execution is explicit and synchronous
// - Subscriptions declare protocol intent; poll() drives all effects
// - Message delivery is strictly driven by poll()
// - The user controls lifecycle and termination
//
// This is the smallest complete Core program.
// ============================================================================

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "wirekrak/core.hpp"
#include "common/cli/symbol.hpp"
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
    // Runtime configuration (no hard-coded behavior)
    // -------------------------------------------------------------------------
    const auto& params = wirekrak::cli::symbol::configure(argc, argv,
        "Wirekrak Core - Minimal Poll-Driven Session (Trade)\n"
        "Demonstrates explicit subscription and poll-driven execution.\n",
        "This example shows that stateful streams do not change Core's execution model.\n"
        "Subscriptions are explicit, and message delivery is driven by poll().\n"
    );
    params.dump("=== Runtime Parameters ===", std::cout);

    // -------------------------------------------------------------------------
    // Signal handling (explicit termination)
    // -------------------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------------------
    // Session setup
    // -------------------------------------------------------------------------
    SessionT session(g_ring);

    if (!session.connect(params.url)) {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Explicit protocol subscription
    // -------------------------------------------------------------------------
    int messages_received = 0;

    (void)session.subscribe(
        trade::Subscribe{ .symbols = params.symbols }
    );

    // -------------------------------------------------------------------------
    // Poll-driven execution loop
    // -------------------------------------------------------------------------
    trade::Response trade_msg;
    int idle_spins = 0;
    bool did_work = false;
    while (running.load(std::memory_order_relaxed) && messages_received < 10) {
        (void)session.poll();   // REQUIRED: drives all Core behavior
        // Drain ALL trade messages produced since the last poll().
        // Messages are never delivered outside poll-driven progress.
        session.drain_trade_messages([&](const trade::Response& msg) {
            std::cout << " -> " << msg << std::endl;
            ++messages_received;
            did_work = true;
        });
        // Yield to avoid busy-waiting when idle
        loop::manage_idle_spins(did_work, idle_spins);
    }

    // -------------------------------------------------------------------------
    // Explicit unsubscription
    // -------------------------------------------------------------------------
    if (session.is_active()) {
        (void)session.unsubscribe(
            trade::Unsubscribe{ .symbols = params.symbols }
        );
    }

    std::cout << "[SUCCESS] Minimal Core lifecycle completed.\n";
    return 0;
}
