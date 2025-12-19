#include <iostream>
#include <chrono>
#include <thread>

#include "wirekrak/winhttp/client.hpp"
#include "wirekrak/protocol/kraken/book/subscribe.hpp"
#include "wirekrak/protocol/kraken/book/unsubscribe.hpp"

using namespace wirekrak;
using namespace wirekrak::protocol::kraken;
using Logger = lcr::log::Logger;
using Level  = lcr::log::Level;


/*
This example shows WireKrak’s control-plane support.
We send a manual ping, receive a pong through a dedicated callback, and measure round-trip latency using both Kraken’s engine
timestamps and the local clock.
This functionality is completely independent of channel subscriptions and is designed for heartbeat and operational monitoring.”
*/

int main() {
    Logger::instance().set_level(Level::Info);

    winhttp::WinClient client;

    // Track when we send the ping (local wall clock)
    auto ping_sent_at = std::chrono::steady_clock::now();

    // ---------------------------------------------------------------------
    // Register status handler
    // ---------------------------------------------------------------------
    /*
    SystemState system;             // Trading engine state
    std::string api_version;        // WebSocket API version (e.g. "v2")
    std::uint64_t connection_id;    // Unique connection identifier
    std::string version;            // WebSocket service version
    */
    client.on_status([&](const status::Update& update) {
        WK_INFO("[STATUS] received update: system=" << to_string(update.system)
                << " api_version=" << update.api_version
                << " connection_id=" << update.connection_id
                << " version=" << update.version << ""); 
    });

    // ---------------------------------------------------------------------
    // Register pong handler
    // ---------------------------------------------------------------------
    client.on_pong([&](const system::Pong& pong) {
        WK_INFO("[PONG] received");

        if (pong.success.has()) {
            WK_INFO("  success: " << std::boolalpha << pong.success.value() << "");
        }
        else {
            WK_INFO("  success: <unknown>");
        }

        if (pong.req_id.has()) {
            WK_INFO("  req_id: " << pong.req_id.value() << "");
        }

        if (!pong.warnings.empty()) {
            WK_INFO("  warnings:");
            for (const auto& w : pong.warnings) {
                WK_INFO("    - " << w << "");
            }
        }

        // -----------------------------------------------------------------
        // RTT measurement (engine timestamps, if present)
        // -----------------------------------------------------------------
        if (pong.time_in.has() && pong.time_out.has()) {
            auto engine_rtt = pong.time_out.value() - pong.time_in.value();
            WK_INFO("  engine RTT: " << engine_rtt << "");
        }

        // -----------------------------------------------------------------
        // Local RTT (wall-clock)
        // -----------------------------------------------------------------
        auto now = std::chrono::steady_clock::now();
        auto local_rtt = std::chrono::duration_cast<std::chrono::milliseconds>(now - ping_sent_at);

        WK_INFO("  local RTT: " << local_rtt.count() << " ms");
    });

    // ---------------------------------------------------------------------
    // Connect and start client
    // ---------------------------------------------------------------------
    if (!client.connect("wss://ws.kraken.com/v2")) {
        return -1;
    }

    // Give the socket a moment to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // ---------------------------------------------------------------------
    // Send ping
    // ---------------------------------------------------------------------
    WK_INFO("[PING] sending ping...");
    client.ping(1);   // req_id = 1

    // ---------------------------------------------------------------------
    // Run for a short time to receive pong
    // ---------------------------------------------------------------------
    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < end_time) {
        client.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    WK_INFO("=== Done ===");
    return 0;
}
