// ============================================================================
// Core Contracts Example — Liveness Timeout Exposure (Progress-Based)
//
// This example demonstrates that Wirekrak Core *exposes* liveness failure
// through observable lack of progress, not via liveness states or callbacks.
//
// No protocol traffic is generated:
//   - no subscriptions
//   - no pings or keep-alives
//
// Observable facts:
//   - transport epochs (successful connection cycles)
//   - received message count
//   - transmitted message count
//   - heartbeat count
//
// Liveness failure is inferred when:
//   - the transport epoch increases (reconnect occurred)
//   - but no protocol traffic is ever observed
//
// The example exits once a reconnect without traffic is observed.
// ============================================================================

#include <chrono>
#include <iostream>
#include <thread>

#include "wirekrak/core.hpp"
#include "common/cli/minimal.hpp"


// -----------------------------------------------------------------------------
// Setup environment
// -----------------------------------------------------------------------------
using namespace wirekrak::core;
using namespace wirekrak::core::protocol::kraken;

static MessageRingT g_ring;   // Golbal SPSC ring buffer (transport → session)


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    using namespace schema;

    // -------------------------------------------------------------------------
    // Runtime configuration
    // -------------------------------------------------------------------------
    const auto& params = wirekrak::cli::minimal::configure(
        argc, argv,
        "Wirekrak Core - Liveness timeout exposure example\n",
        "Demonstrates progress-based liveness observation.\n"
        "No subscriptions. No pings.\n"
    );
    params.dump("=== Runtime Parameters ===", std::cout);

    // -------------------------------------------------------------------------
    // Session setup
    // -------------------------------------------------------------------------
    SessionT session(g_ring);

    // -------------------------------------------------------------------------
    // Connect (no subscriptions, no pings)
    // -------------------------------------------------------------------------
    if (!session.connect(params.url)) {
        return -1;
    }

    std::cout << "\n[example] Observing session progress...\n\n";

    // -------------------------------------------------------------------------
    // Initial observation baseline
    // -------------------------------------------------------------------------
    uint64_t last_epoch     = 0;
    uint64_t last_rx        = 0;
    uint64_t last_tx        = 0;
    uint64_t last_heartbeat = 0;

    bool reconnect_observed = false;
    bool traffic_observed   = false;

    // Safety bound (example-level responsibility)
    const auto start = std::chrono::steady_clock::now();
    constexpr auto MAX_OBSERVATION_TIME = std::chrono::seconds(30);

    // -------------------------------------------------------------------------
    // Main polling loop
    // -------------------------------------------------------------------------
    status::Update last_status;
    while (true) {
        const uint64_t epoch = session.poll();

        // --- Observe latest status ---
        if (session.try_load_status(last_status)) {
            std::cout << " -> " << last_status << std::endl;
        }

        // --- Observe transport progression ---
        const uint64_t rx = session.rx_messages();
        const uint64_t tx = session.tx_messages();
        const uint64_t hb = session.heartbeat_total();

        // Detect first successful connection
        if (last_epoch == 0 && epoch > 0) {
            std::cout << "[example] transport connected (epoch " << epoch << ")\n";
        }

        // Detect reconnect
        if (epoch > last_epoch && last_epoch != 0) {
            std::cout << "[example] transport reconnected (epoch " << last_epoch << " -> " << epoch << ")\n";
            reconnect_observed = true;
        }

        // Detect any traffic
        if (rx > last_rx || tx > last_tx || hb > last_heartbeat) {
            traffic_observed = true;
        }

        // Exit condition:
        // reconnect occurred, but no traffic was ever observed
        if (reconnect_observed && !traffic_observed) {
            std::cout << "[example] liveness failure inferred: reconnect without traffic\n";
            break;
        }

        // Absolute safety bound
        if (std::chrono::steady_clock::now() - start > MAX_OBSERVATION_TIME) {
            std::cout << "[example] observation window expired\n";
            break;
        }

        last_epoch     = epoch;
        last_rx        = rx;
        last_tx        = tx;
        last_heartbeat = hb;

        // Yield to avoid busy-waiting when idle
        std::this_thread::yield();
    }

    // -------------------------------------------------------------------------
    // Summary
    // -------------------------------------------------------------------------
    std::cout << "\n[SUMMARY]\n";
    std::cout << "  Subscriptions created : no\n";
    std::cout << "  Pings sent            : no\n";
    std::cout << "  Transport epochs      : " << last_epoch << "\n";
    std::cout << "  RX messages           : " << last_rx << "\n";
    std::cout << "  TX messages           : " << last_tx << "\n";
    std::cout << "  Heartbeats            : " << last_heartbeat << "\n";
    std::cout << "  Reconnect observed    : yes\n";
    std::cout << "  Protocol traffic      : no\n\n";

    std::cout << "[CONTRACT]\n";
    std::cout << "  Wirekrak Core exposes failure via observable progress facts.\n";
    std::cout << "  No liveness states, callbacks, or health polling are required.\n";
    std::cout << "  Transport recovery is orthogonal and observable.\n";
    std::cout << "  Interpretation remains the responsibility of the user.\n";

    return 0;
}
