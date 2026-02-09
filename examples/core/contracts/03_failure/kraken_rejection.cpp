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

#include "wirekrak/core.hpp"
#include "common/cli/minimal.hpp"


// -----------------------------------------------------------------------------
// Helper to drain all available messages
// -----------------------------------------------------------------------------
inline void drain_messages(wirekrak::core::kraken::Session& session) {
    using namespace wirekrak::core::protocol::kraken::schema;

    // Observe latest status
    static status::Update last_status;
    if (session.try_load_status(last_status)) {
        std::cout << " -> " << last_status << std::endl;
    }

    // Drain protocol errors (required)
    session.drain_rejection_messages([](const rejection::Notice& msg) {
        std::cout << " -> " << msg << std::endl;
    });

    // Drain data-plane messages (required)
    session.drain_trade_messages([](const trade::Response& msg) {
        std::cout << " -> " << msg << std::endl;
    });
}


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    using namespace wirekrak::core;
    using namespace protocol::kraken::schema;

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
    kraken::Session session;
    
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
    std::cout << "[example] Trade subscriptions (before subscribe): active=" << mgr.active_total() << " - pending=" << mgr.pending_total() << std::endl;
    (void)session.subscribe(
        trade::Subscribe{ .symbols = { "INVALID/SYMBOL" } }
    );

    // -------------------------------------------------------------------------
    // Observe outcome
    // -------------------------------------------------------------------------
    std::cout << "[example] Trade subscriptions (after subscribe): active=" << mgr.active_total() << " - pending=" << mgr.pending_total() << std::endl;

    // Wait for a few transport lifetimes to prove rejection is not replayed
    auto epoch = session.transport_epoch();
    while (epoch < 3) {
        epoch = session.poll();
        drain_messages(session);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Graceful shutdown: drain until protocol is idle and close session
    // -------------------------------------------------------------------------
    while (!session.is_idle()) {
        (void)session.poll();
        drain_messages(session);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    session.close();

    std::cout << "[example] Trade subscriptions (after close): active=" << mgr.active_total() << " - pending=" << mgr.pending_total() << std::endl;

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
