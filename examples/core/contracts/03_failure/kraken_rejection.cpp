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


int main(int argc, char** argv) {
    using namespace wirekrak::core;
    namespace schema = protocol::kraken::schema;

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
    session.subscribe(
        schema::trade::Subscribe{ .symbols = { "INVALID/SYMBOL" } },
        [](const schema::trade::ResponseView&) {
            // Should never be called
        }
    );

    // -------------------------------------------------------------------------
    // Observe outcome
    // -------------------------------------------------------------------------
    std::cout << "[example] Trade subscriptions (after subscribe): active=" << mgr.active_total() << " - pending=" << mgr.pending_total() << std::endl;

    // Wait for a few transport lifetimes to prove rejection is not replayed
    auto epoch = session.transport_epoch();
    schema::status::Update last_status;
    schema::rejection::Notice rejection;
    while (epoch < 3) {
        epoch = session.poll();
        // --- Observe latest status ---
        if (session.try_load_status(last_status)) {
            std::cout << " -> " << last_status << std::endl;
        }
        // --- Observe rejections ---
        while (session.pop_rejection(rejection)) {
            std::cout << " -> " << rejection << std::endl;
            std::cout << "[example] Trade subscriptions (after rejection): active=" << mgr.active_total() << " - pending=" << mgr.pending_total() << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Shutdown
    // -------------------------------------------------------------------------
    session.close();

    for (int i = 0; i < 20; ++i) {
        (void)session.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[example] Trade subscriptions (after close): active=" << mgr.active_total() << " - pending=" << mgr.pending_total() << std::endl;

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
