// ============================================================================
// Core Contracts Example — Liveness Failure Exposure (Edge-Based)
//
// This example demonstrates that Wirekrak Core *exposes* liveness failure
// deterministically via connection events.
//
// No protocol traffic is generated:
//   - no subscriptions
//   - no pings or keep-alives
//
// Expected observable sequence:
//
//   - transport connection established
//   - initial protocol status message
//   - LivenessThreatened event
//   - forced transport disconnect (Disconnected)
//
// No protocol-level recovery, masking, or smoothing is performed.
// Transport reconnection may occur, but this example does not react to it.
// ============================================================================


#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "wirekrak/core.hpp"
#include "common/cli/minimal.hpp"


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

    // -----------------------------------------------------------------------------
    // Lifecycle flags (observational only)
    // -----------------------------------------------------------------------------
    static std::atomic<bool> warned{false};
    static std::atomic<bool> disconnected{false};


    // -------------------------------------------------------------------------
    // Session setup
    // -------------------------------------------------------------------------
    Session session;

    // Status handler (shows initial protocol traffic)
    session.on_status([](const schema::status::Update& status) {
        std::cout << " -> " << status << std::endl;
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

    auto handle_transition_event = [](transport::TransitionEvent ev) {
        switch (ev) {
        case transport::TransitionEvent::Connected:
            std::cout << "[example] connected\n";
            break;

        case transport::TransitionEvent::LivenessThreatened:
            std::cout << "[example] liveness warning: approaching timeout\n";
            warned.store(true);
            break;

        case transport::TransitionEvent::Disconnected:
            std::cout << "[example] liveness timeout: forced disconnect\n";
            disconnected.store(true);
            break;

        default:
            break;
        }
    };

    //auto last_liveness = session.liveness();
    while (!disconnected.load()) {
        session.poll();
        transport::TransitionEvent ev;
/*
        while (session.poll_event(ev)) {
            handle_transition_event(ev);
        }
*/
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n[SUMMARY]\n";
    std::cout << "  Subscriptions created : no\n";
    std::cout << "  Pings sent            : no\n";
    std::cout << "  Liveness warning      : " << (warned.load() ? "observed" : "not observed") << "\n";
    std::cout << "  Transport disconnect  : observed\n\n";

    std::cout << "[CONTRACT]\n";
    std::cout << "  Wirekrak Core exposes liveness failure via edge-triggered events.\n";
    std::cout << "  No level-based liveness or health polling is required.\n";
    std::cout << "  Liveness timeout deterministically leads to disconnect.\n";
    std::cout << "  Transport reconnection remains orthogonal and observable.\n";

    return 0;
}
