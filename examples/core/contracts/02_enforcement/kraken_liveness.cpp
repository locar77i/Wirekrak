// ============================================================================
// Core Contracts Example - Protocol Liveness Policy (Kraken Session)
//
// This example demonstrates how Wirekrak enforces connection liveness
// while delegating responsibility for maintaining it to the protocol layer.
//
// CONTRACT DEMONSTRATED:
//
// - transport::Connection enforces liveness *by observation*
// - protocol::kraken::Session decides whether and how to emit traffic
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
//     - Session reacts to LivenessThreatened events
//     - Protocol-level pings are emitted explicitly
//     - Liveness is preserved only if traffic is observed
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


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    using namespace wirekrak::core;
    using namespace protocol::kraken::schema;
    using namespace protocol::policy;

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
    kraken::Session session;

    // -------------------------------------------------------------------------
    // Phase A - Passive liveness
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase A - Passive liveness policy\n";
    std::cout << "          No protocol heartbeats will be sent.\n";
    std::cout << "          Forced reconnects are expected if the protocol remains silent.\n\n";

    session.set_policy(Liveness::Passive);

    if (!session.connect(params.url)) {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Poll-driven execution loop
    // -------------------------------------------------------------------------
    auto phase_a_start = std::chrono::steady_clock::now();
    system::Pong last_pong;
    while (std::chrono::steady_clock::now() - phase_a_start < std::chrono::seconds(20)) {
        (void)session.poll();
        // Observe latest pong (liveness signal - not relevant in Passive policy)
        if (session.try_load_pong(last_pong)) {
            std::cout << " -> " << last_pong << std::endl;
        }
        std::this_thread::yield();
    }

    // -------------------------------------------------------------------------
    // Phase B - Active liveness
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase B - Active liveness policy\n";
    std::cout << "          Session will react to liveness warnings\n";
    std::cout << "          by sending protocol-level pings.\n\n";

    session.set_policy(Liveness::Active);

    while (running.load(std::memory_order_relaxed)) {
        (void)session.poll();
        // Observe latest pong (liveness signal - only relevant in Active policy)
        if (session.try_load_pong(last_pong)) {
            std::cout << " -> " << last_pong << std::endl;
        }
        std::this_thread::yield();
    }

    // -------------------------------------------------------------------------
    // Graceful shutdown: drain until protocol is idle and close session
    // -------------------------------------------------------------------------
    while (!session.is_idle()) {
        (void)session.poll();
        // Observe latest pong (liveness signal - only relevant in Active policy)
        if (session.try_load_pong(last_pong)) {
            std::cout << " -> " << last_pong << std::endl;
        }
        std::this_thread::yield();
    }

    session.close();

    // -------------------------------------------------------------------------
    // Summary
    // -------------------------------------------------------------------------
    std::cout << "\n=== Summary ===\n"
              << "- Passive policy allows forced reconnects.\n"
              << "- Active policy attempts to maintain liveness via protocol pings.\n"
              << "- Connection enforces invariants; protocol provides signals.\n"
              << "- Behavior is explicit, observable, and deterministic.\n\n"
              << "Wirekrak enforces correctness.\n"
              << "Responsibility is explicit and observable.\n";

    return 0;
}
