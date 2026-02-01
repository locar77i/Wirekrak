// ============================================================================
// Core Contracts Example — Control Plane (Ping / Pong)
//
// This example demonstrates Wirekrak Core's control-plane support.
//
// CONTRACT DEMONSTRATED:
//
// - Control-plane messages (ping, pong, status) are independent of subscriptions
// - Pong responses are delivered via a dedicated callback
// - Engine timestamps and local wall-clock time can be correlated
// - No market data subscriptions are required
//
// This functionality is designed for:
// - Heartbeat verification
// - Operational monitoring
// - Connectivity and latency diagnostics
//
// ============================================================================

#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>

#include "wirekrak/core.hpp"
#include "common/cli/minimal.hpp"

int main(int argc, char** argv) {
    using namespace wirekrak::core;
    namespace schema = protocol::kraken::schema;

    std::atomic<bool> pong_received{false};

    // -------------------------------------------------------------------------
    // Runtime configuration (no hard-coded behavior)
    // -------------------------------------------------------------------------
    const auto& params = wirekrak::cli::minimal::configure(argc, argv,
        "Wirekrak Core - Control Plane (Ping / Pong)\n"
        "Demonstrates Wirekrak Core's control-plane support.\n",
        "This example requires no market data subscriptions.\n"
        "It shows ping/pong interaction and status observation.\n"
        "Engine timestamps and local wall-clock time can be correlated.\n"
    );
    params.dump("=== Runtime Parameters ===", std::cout);

    // -------------------------------------------------------------------------
    // Session setup
    // -------------------------------------------------------------------------
    Session session;

    // Capture local wall-clock time at ping send
    auto ping_sent_at = std::chrono::steady_clock::now();

    // -------------------------------------------------------------------------
    // Observe protocol-level status events
    // -------------------------------------------------------------------------
    session.on_status([](const schema::status::Update& status) {
        std::cout << " -> " << status << std::endl;
    });

    // -------------------------------------------------------------------------
    // Observe pong responses
    // -------------------------------------------------------------------------
    session.on_pong([&](const schema::system::Pong& pong) {
        std::cout << " -> " << pong << "\n\n";

        // Engine-measured RTT (if provided by Kraken)
        if (pong.time_in.has() && pong.time_out.has()) {
            auto engine_rtt = pong.time_out.value() - pong.time_in.value();
            std::cout << "    engine RTT: " << engine_rtt.count() << " ns\n";
        }

        // Local RTT using wall-clock
        auto now = std::chrono::steady_clock::now();
        auto local_rtt = std::chrono::duration_cast<std::chrono::milliseconds>(now - ping_sent_at);
        std::cout << "    local RTT : " << local_rtt.count() << " ms\n" << std::endl;

        // Mark pong as received
        pong_received.store(true, std::memory_order_relaxed);
    });

    // -------------------------------------------------------------------------
    // Connect
    // -------------------------------------------------------------------------
    if (!session.connect(params.url)) {
        return -1;
    }

    // -------------------------------------------------------------------------
    // Send control-plane ping
    // -------------------------------------------------------------------------
    std::cout << "[example] Sending ping...\n";
    session.ping(); // req_id is auto-assigned internally (0 is ping-reserved for control-plane)

    // -------------------------------------------------------------------------
    // Poll for a short, bounded observation window
    // -------------------------------------------------------------------------
    while (!pong_received.load(std::memory_order_relaxed)) {
        session.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Graceful shutdown
    // -------------------------------------------------------------------------
    session.close();

    std::cout << "\n[SUCCESS] Control-plane interaction observed.\n";
    return 0;
}
