// ============================================================================
// Core Contracts Example — Minimal Stateful Stream (Order Book)
//
// This example demonstrates that stateful streams (order books) do NOT change
// Wirekrak Core’s execution model.
//
// CONTRACTS DEMONSTRATED:
//
// - Order book subscriptions are explicit protocol requests
// - Statefulness does not imply background execution
// - Message delivery is strictly driven by poll()
// - Lifecycle and termination are fully user-controlled
//
// This example mirrors the minimal trade example, using a stateful stream.
// ============================================================================

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "wirekrak/core/preset/protocol/kraken_default.hpp"
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

static preset::DefaultMessageRing g_ring;   // Golbal SPSC ring buffer (transport → session)


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    
    using namespace schema;
    

    // -------------------------------------------------------------------------
    // Runtime configuration (no hard-coded behavior)
    // -------------------------------------------------------------------------
    const auto& params = wirekrak::cli::symbol::configure(argc, argv,
        "Wirekrak Core - Minimal Poll-Driven Session (Order Book)\n"
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
    preset::protocol::kraken::DefaultSession session(g_ring);

    if (!session.connect(params.url)) {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Explicit protocol subscription (stateful)
    // -------------------------------------------------------------------------
    int messages_received = 0;

    (void)session.subscribe(
        book::Subscribe{ .symbols = params.symbols }
    );

    // -------------------------------------------------------------------------
    // Poll-driven execution loop
    // -------------------------------------------------------------------------
    int idle_spins = 0;
    bool did_work = false;
    while (running.load(std::memory_order_relaxed) && messages_received < 60 && session.is_active()) {
        (void)session.poll();   // REQUIRED: drives all Core behavior
        // Drain book messages in a loop until empty, to ensure we process all messages received in this poll
        session.drain_book_messages([&](const book::Response& msg) {
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
            book::Unsubscribe{ .symbols = params.symbols }
        );
    }

    std::cout << "[SUCCESS] Minimal stateful Core lifecycle completed.\n";
    return 0;
}
