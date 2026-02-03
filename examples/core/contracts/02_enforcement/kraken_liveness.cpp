// ============================================================================
// Core Contracts Example - Protocol Liveness Policy (Kraken Session)
//
// This example demonstrates how Wirekrak enforces connection liveness
// while delegating responsibility for maintaining it to the protocol layer.
//
// CONTRACT DEMONSTRATED:
//
// - The transport::Connection enforces liveness invariants
// - The protocol::kraken::Session decides how to satisfy liveness
// - Liveness behavior is policy-driven (Passive vs Active)
// - Forced reconnects are intentional, observable, and recoverable
//
// POLICIES:
//
//   Passive:
//     - Session observes liveness only
//     - No protocol heartbeats are emitted
//     - Connection may force reconnects
//
//   Active:
//     - Session reacts to liveness warnings
//     - Protocol-level pings are emitted
//     - Observable traffic maintains liveness
//
// ============================================================================

#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

#include "wirekrak/core.hpp"
#include "common/cli/minimal.hpp"

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
    namespace policy = protocol::policy;

    // -------------------------------------------------------------------------
    // Runtime configuration
    // -------------------------------------------------------------------------
    const auto& params = wirekrak::cli::minimal::configure(argc, argv,
        "Wirekrak Core - Kraken Session Liveness Policy\n"
        "Demonstrates Passive vs Active protocol liveness handling.\n",
        "Passive: no protocol heartbeats, reconnects may occur.\n"
        "Active : protocol emits pings to maintain liveness.\n"
    );
    params.dump("=== Runtime Parameters ===", std::cout);

    // -------------------------------------------------------------------------
    // Signal handling (explicit termination)
    // -------------------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------------------
    // Session setup
    // -------------------------------------------------------------------------
    protocol::kraken::Session<transport::winhttp::WebSocket> session;

    std::atomic<int> disconnects{0};


    // -------------------------------------------------------------------------
    // Observe connection lifecycle
    // -------------------------------------------------------------------------
    session.on_disconnect([&] {
        int d = ++disconnects;
    });

    // -------------------------------------------------------------------------
    // Observe pong messages (only relevant in Active policy)
    // -------------------------------------------------------------------------
    session.on_pong([](const schema::system::Pong& pong) {
        std::cout << " -> " << pong << std::endl;
    });

    // -------------------------------------------------------------------------
    // Phase A - Passive liveness
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase A - Passive liveness policy\n";
    std::cout << "          No protocol heartbeats will be sent.\n";
    std::cout << "          Forced reconnects may occur.\n\n";

    session.set_policy(policy::Liveness::Passive);

    if (!session.connect(params.url)) {
        return -1;
    }

    auto phase_a_start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - phase_a_start < std::chrono::seconds(20)) {
        session.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Phase B - Active liveness
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase B - Active liveness policy\n";
    std::cout << "          Session will react to liveness warnings\n";
    std::cout << "          by sending protocol-level pings.\n\n";

    session.set_policy(policy::Liveness::Active);

    while (running.load(std::memory_order_relaxed)) {
        session.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Shutdown
    // -------------------------------------------------------------------------
    session.close();

    for (int i = 0; i < 20; ++i) {
        session.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Summary
    // -------------------------------------------------------------------------
    std::cout << "\n=== Summary ===\n"
              << "- Passive policy allows forced reconnects.\n"
              << "- Active policy maintains liveness via protocol pings.\n"
              << "- Connection enforces invariants; protocol provides signals.\n"
              << "- Behavior is explicit, observable, and deterministic.\n\n"
              << "Wirekrak enforces correctness.\n"
              << "Responsibility is explicit and observable.\n";

    return 0;
}
