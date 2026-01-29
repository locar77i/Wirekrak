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

#include "wirekrak/core.hpp"

// -----------------------------------------------------------------------------
// Lifecycle control
// -----------------------------------------------------------------------------
std::atomic<bool> running{true};

void on_signal(int) {
    running.store(false);
}

int main() {
    using namespace wirekrak::core;
    namespace schema = protocol::kraken::schema;

    // -------------------------------------------------------------------------
    // Signal handling (explicit termination)
    // -------------------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------------------
    // Session setup
    // -------------------------------------------------------------------------
    Session session;

    if (!session.connect("wss://ws.kraken.com/v2")) {
        std::cerr << "Failed to connect\n";
        return -1;
    }

    // -------------------------------------------------------------------------
    // Explicit protocol subscription (stateful)
    // -------------------------------------------------------------------------
    int messages_received = 0;

    session.subscribe(
        schema::book::Subscribe{ .symbols = {"BTC/EUR"} },
        [&](const schema::book::Response& book) {
            std::cout << " -> [BOOK] " << book << std::endl;
            ++messages_received;
        }
    );

    // -------------------------------------------------------------------------
    // Poll-driven execution loop
    // -------------------------------------------------------------------------
    while (running.load(std::memory_order_relaxed) && messages_received < 60) {
        session.poll();   // REQUIRED: drives all Core behavior
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Explicit unsubscription
    // -------------------------------------------------------------------------
    session.unsubscribe(
        schema::book::Unsubscribe{ .symbols = {"BTC/EUR"} }
    );

    // -------------------------------------------------------------------------
    // Observability
    // -------------------------------------------------------------------------
    std::cout << "\n[INFO] Heartbeats observed: "
              << session.heartbeat_total() << std::endl;

    std::cout << "[SUCCESS] Minimal stateful Core lifecycle completed.\n";
    return 0;
}
