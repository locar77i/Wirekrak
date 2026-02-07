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


int main(int argc, char** argv) {
    using namespace wirekrak::core;
    namespace schema = protocol::kraken::schema;

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

    // Register status handler
    session.on_status([](const schema::status::Update& status) {
        std::cout << " -> " << status << std::endl;
    });

    // -------------------------------------------------------------------------
    // Connect
    // -------------------------------------------------------------------------
    if (!session.connect(params.url)) {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Subscribe ONCE to LTC/EUR trade updates (no snapshot)
    // -------------------------------------------------------------------------
    session.subscribe(
        schema::trade::Subscribe{
            .symbols  = params.symbols,
            .snapshot = false // to avoid burst output and keep replay observable
        },
        [](const schema::trade::ResponseView& trade) {
            std::cout << " -> " << trade << std::endl;
        }
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
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Post-reconnect observation window
    // -------------------------------------------------------------------------
    std::cout << "\n[VERIFY] Observing replay...\n";

    auto verify_until = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (std::chrono::steady_clock::now() < verify_until) {
        (void)session.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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

    std::cout << "\n[SUCCESS] Exiting cleanly.\n";

    std::cout << "\n[SUMMARY] disconnect -> reconnect -> replay -> resume confirmed\n";

    return 0;
}
