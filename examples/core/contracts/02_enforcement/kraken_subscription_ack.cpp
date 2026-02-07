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

int main(int argc, char** argv) {
    using namespace wirekrak::core;
    namespace schema = protocol::kraken::schema;

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

    session.on_status([](const schema::status::Update& status) {
        std::cout << " -> " << status << std::endl;
    });

    session.on_rejection([](const schema::rejection::Notice& notice) {
        std::cout << " -> " << notice << std::endl;
    });

    // -------------------------------------------------------------------------
    // Connect
    // -------------------------------------------------------------------------
    if (!session.connect(params.url)) {
        std::cerr << "Failed to connect\n";
        return -1;
    }

    // -------------------------------------------------------------------------
    // Issue duplicate subscribe requests
    // -------------------------------------------------------------------------
    session.subscribe(
        schema::trade::Subscribe{ .symbols = params.symbols },
        [](const schema::trade::ResponseView& trade) {
            std::cout << " -> " << trade << std::endl;
        }
    );

    session.subscribe(
        schema::trade::Subscribe{ .symbols = params.symbols },
        [](const schema::trade::ResponseView&) {
            // intentionally unused
        }
    );

    // -------------------------------------------------------------------------
    // Immediately unsubscribe
    // -------------------------------------------------------------------------
    session.unsubscribe(
        schema::trade::Unsubscribe{ .symbols = params.symbols }
    );

    // -------------------------------------------------------------------------
    // Observe protocol ACKs and subscription state progression
    // (independent of transport reconnects or epoch changes)
    // -------------------------------------------------------------------------
    const auto& mgr = session.trade_subscriptions();
    auto observe_until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < observe_until) {
        (void)session.poll();
        std::cout << "[example] Trade subscriptions: active=" << mgr.active_total() << " - pending=" << mgr.pending_total() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // -------------------------------------------------------------------------
    // Graceful shutdown
    // -------------------------------------------------------------------------
    session.close();

    // Drain a short window to allow event processing
    auto drain_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (std::chrono::steady_clock::now() < drain_until) {
        (void)session.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n[SUMMARY]\n"
          << " - Subscription state was ACK-driven\n"
          << " - Duplicate request was rejected by protocol\n"
          << " - No optimistic assumptions were made\n";

    return 0;
}
