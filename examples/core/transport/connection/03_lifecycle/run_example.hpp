#pragma once

/*
===============================================================================
Example 3 - Error & Close Lifecycle
===============================================================================

This example validates Wirekrak's lifecycle invariants by observing how
errors and close events propagate through the transport and connection layers.

Scenario:
  1) Connect to a WebSocket endpoint
  2) Trigger an abnormal shutdown (transport error or remote close)
  3) Observe callback ordering
  4) Dump connection and transport telemetry

What this teaches:
  - Exactly-once close semantics
  - Error-before-close ordering
  - No double counting of lifecycle events

Key takeaway:
  Lifecycle telemetry reflects *what actually happened* - once, in order,
  and without ambiguity.

===============================================================================
*/

#include <iostream>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>

#include "wirekrak/core/transport/connection.hpp"
#include "wirekrak/core/transport/winhttp/websocket.hpp"

using WS         = wirekrak::core::transport::winhttp::WebSocket;
using Connection = wirekrak::core::transport::Connection<WS>;

inline int run_example(const char* name, const char* url, const char* description, const char* subscribe_payload,
                       std::chrono::seconds runtime = std::chrono::seconds(5)) {
    std::cout << "=== Wirekrak Connection - Error & Close Lifecycle (" << name << ") ===\n\n";
    std::cout << description << "\n\n";

    wirekrak::core::transport::telemetry::Connection telemetry;
    Connection conn(telemetry);

    // ---------------------------------------------------------------------
    // Callbacks (order matters!)
    // ---------------------------------------------------------------------

    conn.on_connect([] {
        std::cout << "[example] Connected\n";
    });

    conn.on_message([&](std::string_view msg) {
        std::cout << "[example] RX message (" << msg.size() << " bytes)\n";
    });

    conn.on_disconnect([] {
        std::cout << "[example] Disconnected (exactly once)\n";
    });

    conn.on_retry([](const wirekrak::core::transport::RetryContext& ctx) {
        std::cout << "[example] Retry scheduled after error: "
                  << to_string(ctx.error)
                  << " (attempt " << ctx.attempt << ")\n";
    });

    // ---------------------------------------------------------------------
    // Open connection
    // ---------------------------------------------------------------------

    std::cout << "[example] Connecting to " << url << "\n";
    if (conn.open(url) != wirekrak::core::transport::Error::None) {
        std::cerr << "[example] Failed to connect\n";
        return 1;
    }

    // ---------------------------------------------------------------------
    // Send subscription (if any)
    // ---------------------------------------------------------------------

    if (subscribe_payload && subscribe_payload[0] != '\0') {
        std::cout << "[example] Sending subscription\n";
        (void)conn.send(subscribe_payload);
    }

    // ---------------------------------------------------------------------
    // Observation window
    // ---------------------------------------------------------------------

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < runtime) {
        conn.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // ---------------------------------------------------------------------
    // Force local shutdown (idempotent)
    // ---------------------------------------------------------------------

    std::cout << "[example] Initiating local close()\n";
    conn.close();

    for (int i = 0; i < 20; ++i) {
        conn.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // ---------------------------------------------------------------------
    // Dump telemetry
    // ---------------------------------------------------------------------

    std::cout << "\n=== Connection Telemetry ===\n";
    telemetry.debug_dump(std::cout);

    std::cout << "\n=== WebSocket Telemetry ===\n";
    telemetry.websocket.debug_dump(std::cout);

    // ---------------------------------------------------------------------
    // Interpretation guide
    // ---------------------------------------------------------------------

    std::cout << "\n=== How to read this ===\n"

              << "Lifecycle\n"
              << "  Disconnect events\n"
              << "    Logical connection shutdowns observed by Connection.\n"
              << "    This must always be exactly once.\n\n"

              << "Errors / lifecycle (WebSocket)\n"
              << "  Receive errors\n"
              << "    Transport-level failures during receive.\n"
              << "    These occur before closure, not instead of it.\n\n"

              << "Close events\n"
              << "  Close events\n"
              << "    Physical WebSocket closures.\n"
              << "    These are idempotent and never double-counted.\n\n"

              << "Key invariant:\n"
              << "  Errors may occur before close,\n"
              << "  but close is always observed exactly once.\n";

    return 0;
}
