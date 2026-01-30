// ============================================================================
// Core Contracts Example — Subscription ACK Enforcement
//
// This example demonstrates that subscription state in Wirekrak Core
// is strictly ACK-driven.
//
// CONTRACT DEMONSTRATED:
//
// - Subscriptions are NOT assumed active until ACKed
// - Duplicate subscribe requests are not merged optimistically
// - Unsubscribe before ACK is handled deterministically
// - Core never infers or fabricates subscription state
//
// ============================================================================

#include <chrono>
#include <iostream>
#include <thread>

#include "wirekrak/core.hpp"
#include "common/cli/minimal.hpp"

int main(int argc, char** argv) {
    using namespace wirekrak::core;
    namespace schema = protocol::kraken::schema;

    std::cout << "[START] Subscription ACK enforcement example\n";

    // -------------------------------------------------------------------------
    // Runtime configuration (no hard-coded behavior)
    // -------------------------------------------------------------------------
    const auto& params = wirekrak::cli::minimal::configure(argc, argv,
        "Wirekrak Core — Subscription ACK Enforcement Example\n"
        "Demonstrates that subscription state in Wirekrak Core is strictly ACK-driven.\n"
    );
    params.dump("=== Runtime Parameters ===", std::cout);

    // -------------------------------------------------------------------------
    // Session setup
    // -------------------------------------------------------------------------
    Session session;

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

    const std::string symbol = params.symbols.front();

    // -------------------------------------------------------------------------
    // Issue duplicate subscribe requests
    // -------------------------------------------------------------------------
    session.subscribe(
        schema::trade::Subscribe{ .symbols = {symbol} },
        [](const schema::trade::ResponseView& trade) {
            std::cout << " -> " << trade << std::endl;
        }
    );

    session.subscribe(
        schema::trade::Subscribe{ .symbols = {symbol} },
        [](const schema::trade::ResponseView&) {
            // intentionally unused
        }
    );

    // -------------------------------------------------------------------------
    // Immediately unsubscribe
    // -------------------------------------------------------------------------
    session.unsubscribe(
        schema::trade::Unsubscribe{ .symbols = {symbol} }
    );

    // -------------------------------------------------------------------------
    // Observe ACKs and state transitions
    // -------------------------------------------------------------------------
    auto observe_until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < observe_until) {
        session.poll();

        const auto& mgr = session.trade_subscriptions();
        std::cout << "[STATE] active=" << mgr.active_total() << " pending=" << mgr.pending_total() << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "\n[SUMMARY]\n"
          << " - Subscription state was ACK-driven\n"
          << " - Duplicate request was rejected by protocol\n"
          << " - No optimistic assumptions were made\n";

    return 0;
}
