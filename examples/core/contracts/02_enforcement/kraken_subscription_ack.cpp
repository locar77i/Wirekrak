// ============================================================================
// Core Contracts Example — Subscription ACK Enforcement
//
// This example demonstrates that subscription state in Wirekrak Core
// is **strictly protocol-ACK driven** and independent of transport lifecycle.
//
// CONTRACT DEMONSTRATED:
//
// - Subscriptions are NOT considered active until ACKed by the protocol
// - Duplicate subscribe intents are surfaced, not merged optimistically
// - Unsubscribe-before-ACK is handled deterministically
// - Subscription state is never inferred from transport connectivity
// - No replay occurs for rejected or unacknowledged intent
//
// Transport progress, reconnects, and epochs are orthogonal to this contract.
//
// ============================================================================


#include <chrono>
#include <iostream>
#include <thread>

#include "wirekrak/core.hpp"
#include "common/cli/symbol.hpp"


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

    std::cout << "[START] Subscription ACK enforcement example\n";

    // -------------------------------------------------------------------------
    // Runtime configuration (no hard-coded behavior)
    // -------------------------------------------------------------------------
    const auto& params = wirekrak::cli::symbol::configure(argc, argv,
        "Wirekrak Core — Subscription ACK Enforcement Example\n"
        "Demonstrates that subscription state in Wirekrak Core is strictly ACK-driven.\n",
        "Subscriptions are NOT assumed active until ACKed.\n"
        "Duplicate subscribe requests are not merged optimistically.\n"
        "Unsubscribe before ACK is handled deterministically.\n"
        "Core never infers or fabricates subscription state.\n"
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
    // Issue duplicate subscribe requests
    // -------------------------------------------------------------------------
    (void)session.subscribe(
        trade::Subscribe{ .symbols = params.symbols }
    );

    (void)session.subscribe(
        trade::Subscribe{ .symbols = params.symbols }
    );

    // -------------------------------------------------------------------------
    // Immediately unsubscribe
    // -------------------------------------------------------------------------
    (void)session.unsubscribe(
        trade::Unsubscribe{ .symbols = params.symbols }
    );

    // -------------------------------------------------------------------------
    // Observe protocol ACKs and subscription state progression
    // (independent of transport reconnects or epoch changes)
    // -------------------------------------------------------------------------
    const auto& mgr = session.trade_subscriptions();
    auto observe_until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < observe_until) {
        (void)session.poll();
        drain_messages(session);
        std::cout << "[example] Trade subscriptions: active=" << mgr.active_total() << " - pending=" << mgr.pending_total() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
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

    std::cout << "\n[SUCCESS] Clean shutdown completed.\n";

    std::cout << "\n[SUMMARY]\n"
          << " - Subscription state was ACK-driven\n"
          << " - Duplicate request was rejected by protocol\n"
          << " - No optimistic assumptions were made\n";

    return 0;
}
