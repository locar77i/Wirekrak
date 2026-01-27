#pragma once

/*
===============================================================================
Example 4 - Heartbeat-driven Liveness (Protocol-managed)
===============================================================================

This example demonstrates Wirekrak’s liveness model and the strict separation
of responsibilities between the Connection layer and protocol logic.

The Connection enforces liveness:
  - It requires observable application-level traffic to consider a connection
    healthy.
  - In the absence of such traffic, it deterministically forces a reconnect.

The Protocol satisfies liveness:
  - By emitting periodic messages (e.g. protocol pings),
    it provides the signals required to keep the connection alive.

The example runs in three phases:

Phase A - Passive observation
  - A WebSocket connection is opened without subscriptions or pings.
  - The exchange may emit initial system messages, then become silent.
  - Once no traffic is observed within the liveness window,
    the Connection force-closes and reconnects.

Phase B - Message observation without protocol support
  - A message callback is registered to observe incoming messages.
  - Initial system messages are now forwarded and visible to the application.
  - Despite observation, the absence of ongoing traffic still triggers
    a forced reconnect once the liveness window expires.

Phase C - Protocol-managed heartbeat
  - After a configurable number of reconnects, the protocol begins sending
    periodic ping messages.
  - Each ping elicits a server response.
  - Observable traffic resumes and the connection stabilizes.

Key lessons:
  - Liveness is not automatic and is never inferred.
  - Passive WebSocket connections are not guaranteed to remain healthy.
  - Observability alone does not imply liveness.
  - The Connection enforces health invariants.
  - Protocols are responsible for producing liveness signals.
  - Forced reconnects are intentional, observable, and recoverable.

This example intentionally makes no assumptions about exchange behavior.
It reports exactly what happens on the wire.

Wirekrak enforces correctness - it does not hide responsibility.
===============================================================================
*/


#include <iostream>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>
#include <csignal>

#include "wirekrak/core/transport/connection.hpp"
#include "wirekrak/core/transport/winhttp/websocket.hpp"

// WebSocket transport specialization
using WS = wirekrak::core::transport::winhttp::WebSocket;

// -----------------------------------------------------------------------------
// Ctrl+C handling
// -----------------------------------------------------------------------------
static std::atomic<bool> running{true};

inline void on_signal(int) {
    running.store(false);
}

// -----------------------------------------------------------------------------
// Runner
// -----------------------------------------------------------------------------

inline int run_example(
    const char* name,
    const char* url,
    const char* description,
    const char* ping_payload = nullptr,
    int enable_ping_after_reconnects = 0,
    std::chrono::seconds observe_for = std::chrono::seconds(10)
) {
    std::signal(SIGINT, on_signal);

    std::cout
        << "=== Wirekrak Connection - Heartbeat-driven Liveness (" << name << ") ===\n\n"
        << description << "\n\n";

    // -------------------------------------------------------------------------
    // Connection setup
    // -------------------------------------------------------------------------
    wirekrak::core::transport::telemetry::Connection telemetry;
    wirekrak::core::transport::Connection<WS> connection(telemetry);

    std::atomic<uint64_t> forwarded{0};
    std::atomic<int> reconnects{0};
    std::atomic<bool> ping_enabled{false};
    std::atomic<bool> connected{false};

    connection.on_connect([&] {
        connected.store(true, std::memory_order_relaxed);
        std::cout << "[example] Connected to " << name << " WebSocket\n";
    });

    connection.on_disconnect([&] {
        connected.store(false, std::memory_order_relaxed);
        std::cout << "[example] Disconnected\n";
    });

    connection.on_retry([&](const wirekrak::core::transport::RetryContext& rc) {
        std::cout << "[example] Retry context -> url '" << rc.url << "'"
                  << ", attempt " << rc.attempt << ", delay " << rc.next_delay << " ms, error '" << wirekrak::core::transport::to_string(rc.error) << "'\n";
        const int r = ++reconnects;
        if (ping_payload && enable_ping_after_reconnects > 0 &&
            r >= enable_ping_after_reconnects) {
            ping_enabled.store(true);
        }
    });

    // -------------------------------------------------------------------------
    // Open connection
    // -------------------------------------------------------------------------
    std::cout << "[example] Connecting to " << url << "\n";
    if (connection.open(url) != wirekrak::core::transport::Error::None) {
        std::cerr << "[example] Failed to connect\n";
        return 1;
    }

    // -------------------------------------------------------------------------
    // Phase A - passive observation
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase A - passive observation\n";

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < observe_for && running.load(std::memory_order_relaxed)) {
        connection.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Phase B - install message callback
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase B - installing message callback\n";

    connection.on_message([&](std::string_view msg) {
        ++forwarded;
        std::cout << "[example] Forwarded message: " << msg << " (" << msg.size() << " bytes)\n";
    });

    start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < observe_for && running.load(std::memory_order_relaxed)) {
        connection.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Phase C - protocol-managed heartbeat
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase C - protocol-managed heartbeat\n";

    auto last_ping = std::chrono::steady_clock::now();

    while (running.load(std::memory_order_relaxed)) {
        connection.poll();

        if (ping_enabled.load() && ping_payload) {
            auto now = std::chrono::steady_clock::now();
            if (now - last_ping > std::chrono::seconds(10) && connected.load(std::memory_order_relaxed)) {
                std::cout << "[example] Sending protocol ping\n";
                (void)connection.send(ping_payload);
                last_ping = now;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Close connection
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Closing connection\n";
    connection.close();

    for (int i = 0; i < 20; ++i) {
        connection.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Dump telemetry
    // -------------------------------------------------------------------------
    std::cout << "\n=== Connection Telemetry ===\n";
    telemetry.debug_dump(std::cout);

    std::cout << "\n=== WebSocket Telemetry ===\n";
    telemetry.websocket.debug_dump(std::cout);

    // -------------------------------------------------------------------------
    // Interpretation guide
    // -------------------------------------------------------------------------
    std::cout << "\n=== Key observations ===\n"
        << "- Passive connections may fail liveness.\n"
        << "- Forced reconnects are intentional and observable.\n"
        << "- Protocol pings restore liveness stability.\n"
        << "- Connection enforces health; protocol provides signals.\n\n"
        << "Wirekrak reports reality - it does not hide responsibility.\n";

    return 0;
}
