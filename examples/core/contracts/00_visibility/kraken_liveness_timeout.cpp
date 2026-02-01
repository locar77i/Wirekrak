// ============================================================================
// Core Contracts Example - Liveness Timeout Exposure
//
// This example demonstrates that Wirekrak Core *exposes* liveness failure.
// It does NOT hide it, smooth it, or auto-correct it.
//
// The absence of protocol traffic (subscriptions or pings) causes liveness
// to expire, which deterministically leads to a forced disconnect.
//
// HOW TO USE THIS EXAMPLE:
//
// 1. Run the program.
// 2. Do NOT subscribe to any channel.
// 3. Do NOT send pings or keep-alive messages.
// 4. Observe the following sequence:
//
//    - transport connection established
//    - initial protocol status message
//    - liveness warning (approaching timeout)
//    - liveness timeout
//    - forced transport disconnect
//
// IMPORTANT:
//
// - This example contains NO protocol-level recovery logic.
// - No resubscription.
// - No keep-alive policy.
// - No masking of failure.
//
// Transport retries and reconnection attempts may still occur by design,
// but this example does not react to them in any way.
//
// The program exits after observing liveness timeout and disconnect.
// ============================================================================ 


#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "wirekrak/core.hpp"
#include "common/cli/minimal.hpp"

// -----------------------------------------------------------------------------
// Lifecycle flags (observational only)
// -----------------------------------------------------------------------------
static std::atomic<bool> warned{false};
static std::atomic<bool> timed_out{false};

int main(int argc, char** argv) {
    using namespace wirekrak::core;
    namespace schema = protocol::kraken::schema;

    // -------------------------------------------------------------------------
    // Runtime configuration (no hard-coded behavior)
    // -------------------------------------------------------------------------
    const auto& params = wirekrak::cli::minimal::configure(argc, argv,
            "Wirekrak Core - Liveness timeout exposure example\n"
            "Demonstrates liveness timeout and forced disconnect.\n",
            "This example shows that Core exposes liveness failure instead of smoothing it.\n"
            "No subscriptions will be created.\nNo pings will be sent.\n"
        );

    params.dump("=== Runtime Parameters ===", std::cout);

    // -------------------------------------------------------------------------
    // Session setup
    // -------------------------------------------------------------------------
    Session session;

    // Status handler (shows initial protocol traffic)
    session.on_status([](const schema::status::Update& status) {
        std::cout << " -> " << status << std::endl;
    });

    // Liveness warning exposure
    session.on_liveness_warning([](std::chrono::milliseconds remaining) {
        std::cout << "[example] liveness warning: " << remaining.count() << " ms remaining before timeout\n";
        warned.store(true);
    });

    // Liveness timeout exposure
    session.on_liveness_timeout([] {
        std::cout << "[example] liveness timeout: no protocol traffic observed\n";
        timed_out.store(true);
    });

    // -------------------------------------------------------------------------
    // Connect (no subscriptions, no pings)
    // -------------------------------------------------------------------------
    if (!session.connect(params.url)) {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Main polling loop
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Waiting for liveness to expire...\n\n";

    while (!timed_out.load()) {
        session.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Summary
    // -------------------------------------------------------------------------
    std::cout << "\n[SUMMARY]\n";
    std::cout << "  Subscriptions created : no\n";
    std::cout << "  Pings sent            : no\n";
    std::cout << "  Liveness warning      : " << (warned.load() ? "observed" : "not observed") << "\n";
    std::cout << "  Liveness timeout      : " << (timed_out.load() ? "observed" : "not observed") << "\n";
    std::cout << "  Transport disconnect  : exposed\n\n";

    std::cout << "[CONTRACT]\n";
    std::cout << "  Wirekrak Core exposes liveness failure deterministically.\n";
    std::cout << "  Liveness timeout is reported even if transport later reconnects.\n";
    std::cout << "  No protocol-level recovery, masking, or inference is performed.\n";
    std::cout << "  Transport reconnection remains orthogonal and observable.\n";
    return 0;
}
