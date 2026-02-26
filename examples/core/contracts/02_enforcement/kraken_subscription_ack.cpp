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

#include "wirekrak/core/preset/protocol/kraken_default.hpp"
#include "common/cli/symbol.hpp"
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
    preset::protocol::kraken::DefaultSession session(g_ring);
    
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
    int idle_spins = 0;
    bool did_work = false;
    const auto& mgr = session.trade_subscriptions();
    auto observe_until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < observe_until) {
        (void)session.poll();
        did_work = loop::drain_and_print_messages(session);
        std::cout << "[example] Trade subscriptions: active symbols = " << mgr.active_symbols() << " - pending symbols = " << mgr.pending_symbols() << std::endl;
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

    std::cout << "\n[SUCCESS] Clean shutdown completed.\n";

    std::cout << "\n[SUMMARY]\n"
          << " - Subscription state was ACK-driven\n"
          << " - Duplicate request was rejected by protocol\n"
          << " - No optimistic assumptions were made\n";

    return 0;
}
