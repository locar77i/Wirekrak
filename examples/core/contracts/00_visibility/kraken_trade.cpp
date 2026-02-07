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
// - Subscriptions are protocol requests, not background tasks
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

// -----------------------------------------------------------------------------
// Lifecycle control
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}

int main(int argc, char** argv) {
    using namespace wirekrak::core;
    namespace schema = protocol::kraken::schema;

    // -------------------------------------------------------------------------
    // Runtime configuration (no hard-coded behavior)
    // -------------------------------------------------------------------------
    const auto& params = wirekrak::cli::symbol::configure(argc, argv,
        "Wirekrak Core - Minimal Stateful Stream (Trade)\n"
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
    Session session;

    if (!session.connect(params.url)) {
        std::cerr << "Failed to connect\n";
        return -1;
    }

    // -------------------------------------------------------------------------
    // Explicit protocol subscription
    // -------------------------------------------------------------------------
    int messages_received = 0;

    session.subscribe(
        schema::trade::Subscribe{ .symbols = params.symbols },
        [&](const schema::trade::ResponseView& trade) {
            std::cout << " -> [TRADE] " << trade << std::endl;
            ++messages_received;
        }
    );

    // -------------------------------------------------------------------------
    // Poll-driven execution loop
    // -------------------------------------------------------------------------
    while (running.load(std::memory_order_relaxed) && messages_received < 10) {
        (void)session.poll();   // REQUIRED: drives all Core behavior
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Explicit unsubscription
    // -------------------------------------------------------------------------
    session.unsubscribe(
        schema::trade::Unsubscribe{ .symbols = params.symbols }
    );

    // -------------------------------------------------------------------------
    // Observability
    // -------------------------------------------------------------------------
    std::cout << "\n[INFO] Heartbeats observed: "
              << session.heartbeat_total() << std::endl;

    std::cout << "[SUCCESS] Minimal Core lifecycle completed.\n";
    return 0;
}
