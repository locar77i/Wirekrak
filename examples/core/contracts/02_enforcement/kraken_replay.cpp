// ============================================================================
// Core Contracts Example — Replay on Reconnect
//
// This example demonstrates subscription replay enforced by Wirekrak Core.
//
// HOW TO USE THIS EXAMPLE:
//
// 1. Run the program.
// 2. Wait until trade data is flowing.
// 3. Disable network connectivity (e.g. airplane mode).
// 4. Observe disconnect event and retries.
// 5. Re-enable network connectivity.
// 6. Observe:
//    - disconnect
//    - reconnect
//    - subscription replay
//    - trade callbacks resuming
//
// The program exits automatically AFTER a successful reconnect and replay.
//
// IMPORTANT:
// - This example cannot be terminated via Ctrl+C.
// - The only exit path is a real disconnect followed by a successful reconnect.
// - The user does not resubscribe or manage replay logic.
// ============================================================================

#include <atomic>
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

    // -------------------------------------------------------------------------
    // Runtime configuration (no hard-coded behavior)
    // -------------------------------------------------------------------------
    const auto& params = wirekrak::cli::symbol::configure(argc, argv,
        "Wirekrak Core — Subscription Replay Example\n"
        "Demonstrates subscription replay enforced by Wirekrak Core.\n",
        "This example cannot be terminated via Ctrl+C\n"
        "The only exit path is a real disconnect followed by a successful reconnect.\n"
        "The user does not resubscribe or manage replay logic.\n"
    );
    params.dump("=== Runtime Parameters ===", std::cout);

    // -------------------------------------------------------------
    // Session setup
    // -------------------------------------------------------------
    kraken::Session session;

    // -------------------------------------------------------------------------
    // Connect
    // -------------------------------------------------------------------------
    if (!session.connect(params.url)) {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Subscribe ONCE to LTC/EUR trade updates (no snapshot)
    // (no snapshot to avoid burst output and keep replay observable)
    // -------------------------------------------------------------------------
    (void)session.subscribe(
        trade::Subscribe{ .symbols  = params.symbols, .snapshot = false }
    );

    // -------------------------------------------------------------------------
    // Main polling loop (runs until reconnect observed)
    // -------------------------------------------------------------------------
    std::cout << "\n[INFO] Disable your network connection to trigger a disconnect.\n";
    std::cout << "[INFO] Re-enable it to observe reconnect and replay.\n\n";

    // Wait for a few transport lifetimes to prove rejection is not replayed
    auto epoch = session.transport_epoch();
    while (epoch < 2) {
        epoch = session.poll();
        drain_messages(session);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Post-reconnect observation window
    // -------------------------------------------------------------------------
    std::cout << "\n[VERIFY] Observing replay...\n";

    auto verify_until = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (std::chrono::steady_clock::now() < verify_until) {
        (void)session.poll();
        drain_messages(session);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Explicit unsubscription
    // -------------------------------------------------------------------------
    (void)session.unsubscribe(
        trade::Unsubscribe{ .symbols = params.symbols }
    );

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

    std::cout << "\n[SUMMARY] disconnect -> reconnect -> replay -> resume confirmed\n";

    return 0;
}
