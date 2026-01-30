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

#include "wirekrak/core.hpp"
#include "common/logger.hpp"

int main() {
    using namespace wirekrak::core;
    namespace schema = protocol::kraken::schema;

    // -------------------------------------------------------------------------
    // hard-coded configuration
    // -------------------------------------------------------------------------
    wirekrak::log::set_level("info");

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
        std::cout << " -> [STATUS] " << status << std::endl;
    });

    // -------------------------------------------------------------------------
    // Observe pong responses
    // -------------------------------------------------------------------------
    session.on_pong([&](const schema::system::Pong& pong) {
        std::cout << " -> [PONG] " << pong << std::endl;

        // Engine-measured RTT (if provided by Kraken)
        if (pong.time_in.has() && pong.time_out.has()) {
            auto engine_rtt = pong.time_out.value() - pong.time_in.value();
            std::cout << "    engine RTT: " << engine_rtt << std::endl;
        }

        // Local RTT using wall-clock
        auto now = std::chrono::steady_clock::now();
        auto local_rtt =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - ping_sent_at);

        std::cout << "    local RTT: " << local_rtt.count() << " ms" << std::endl;
    });

    // -------------------------------------------------------------------------
    // Connect
    // -------------------------------------------------------------------------
    if (!session.connect("wss://ws.kraken.com/v2")) {
        std::cerr << "Failed to connect\n";
        return -1;
    }

    // Allow connection handshake to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // -------------------------------------------------------------------------
    // Send control-plane ping
    // -------------------------------------------------------------------------
    std::cout << "[PING] sending ping\n";
    session.ping(1);   // explicit req_id for observability

    // -------------------------------------------------------------------------
    // Poll for a short, bounded observation window
    // -------------------------------------------------------------------------
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < until) {
        session.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n[SUCCESS] Control-plane interaction observed.\n";
    return 0;
}
