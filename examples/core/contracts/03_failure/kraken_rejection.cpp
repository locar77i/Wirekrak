// ============================================================================
// Core Contracts Example - Rejection Is Final (No Replay)
//
// This example demonstrates that protocol-level rejections in Wirekrak
// are authoritative and never repaired or retried.
//
// CONTRACT DEMONSTRATED:
//
// - Rejections are surfaced, not repaired
// - Invalid requests are not retried
// - No symbols are dropped or corrected implicitly
// - Transport state remains stable
//
// ============================================================================

#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

#include "wirekrak/core/preset/protocol/kraken_default.hpp"
#include "common/cli/minimal.hpp"
#include "common/loop/helpers.hpp"


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
    // Runtime configuration
    // -------------------------------------------------------------------------
    const auto& params = wirekrak::cli::minimal::configure(
        argc, argv,
        "Wirekrak Core - Rejection Is Final\n",
        "Demonstrates that protocol rejections are surfaced and never retried.\n"
    );
    params.dump("=== Runtime Parameters ===", std::cout);

    // -------------------------------------------------------------------------
    // Session setup
    // -------------------------------------------------------------------------
    preset::protocol::kraken::DefaultSession session(g_ring);
    
    // -------------------------------------------------------------------------
    // Connect
    // -------------------------------------------------------------------------
    if (!session.connect(params.url)) {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Attempt invalid subscription
    // -------------------------------------------------------------------------
    const auto& mgr = session.trade_subscriptions();
    std::cout << "[example] Trade subscriptions (before subscribe): active symbols = " << mgr.active_symbols() << " - pending symbols = " << mgr.pending_symbols() << std::endl;
    (void)session.subscribe(
        trade::Subscribe{ .symbols = { "INVALID/SYMBOL" } }
    );

    // -------------------------------------------------------------------------
    // Observe outcome
    // -------------------------------------------------------------------------
    std::cout << "[example] Trade subscriptions (after subscribe): active symbols = " << mgr.active_symbols() << " - pending symbols = " << mgr.pending_symbols() << std::endl;

    int idle_spins = 0;
    bool did_work = false;
    // Wait for a few transport lifetimes to prove rejection is not replayed
    auto epoch = session.transport_epoch();
    while (epoch < 3) {
        epoch = session.poll();
        did_work = loop::drain_and_print_messages(session);
        loop::manage_idle_spins(did_work, idle_spins);
    }

    // -------------------------------------------------------------------------
    // Graceful shutdown: drain until protocol is idle and close session
    // -------------------------------------------------------------------------
    while (!session.is_idle()) {
        (void)session.poll();
        loop::drain_and_print_messages(session);
        std::this_thread::yield();
    }

    session.close();

    // -------------------------------------------------------------------------
    // Dump telemetry
    // -------------------------------------------------------------------------
    session.telemetry().debug_dump(std::cout);

    std::cout << "[example] Trade subscriptions (after close): active symbols = " << mgr.active_symbols() << " - pending symbols = " << mgr.pending_symbols() << std::endl;

    std::cout << "\n[SUCCESS] Clean shutdown completed.\n";

    // -------------------------------------------------------------------------
    // Summary
    // -------------------------------------------------------------------------
    std::cout << "\n=== Summary ===\n"
            << "- Invalid subscription was rejected by the protocol\n"
            << "- Rejection was final and authoritative\n"
            << "- Rejected intent was NOT replayed after reconnect\n"
            << "- Replay only applies to acknowledged subscriptions\n"
            << "- Transport reconnects occurred independently\n"
            << "- Connection lifecycle remained independent of protocol rejection\n\n"
            << "Wirekrak reports protocol truth - it does not repair intent.\n\n";

    return 0;
}
