#pragma once

/*
===============================================================================
Example 4 - Heartbeat-driven Liveness (Protocol-managed)
===============================================================================

This example demonstrates Wirekrak’s liveness model and the strict separation
of responsibilities between the Connection layer and protocol logic.

It also introduces **liveness warnings** as a cooperative mechanism between
the Connection and the Protocol.

-------------------------------------------------------------------------------
Responsibilities
-------------------------------------------------------------------------------

The Connection enforces liveness:
  - It requires observable application-level traffic to consider a connection
    healthy.
  - In the absence of such traffic, it deterministically forces a reconnect.
  - It emits an early warning when liveness is about to expire.

The Protocol satisfies liveness:
  - By reacting to liveness warnings and emitting protocol-specific messages
    (e.g. pings), it provides the signals required to keep the connection alive.
  - The protocol decides *if* and *when* to act — never the Connection.

-------------------------------------------------------------------------------
Execution phases
-------------------------------------------------------------------------------

Phase A - Passive observation
  - A WebSocket connection is opened without subscriptions or pings.
  - The exchange may emit initial system messages, then become silent.
  - Once no traffic is observed within the liveness window,
    the Connection force-closes and reconnects.

Phase B - Message observation without protocol support
  - A message callback is registered to observe incoming messages.
  - Initial system messages are now forwarded and visible to the application.
  - Despite observability, the absence of ongoing traffic still triggers
    forced reconnects when liveness expires.

Phase C - Protocol-managed liveness (reactive)
  - A liveness warning hook is installed.
  - When the Connection signals that liveness is nearing expiration,
    the protocol reacts by sending a ping.
  - The server responds, observable traffic resumes,
    and forced reconnects are avoided.

-------------------------------------------------------------------------------
Key lessons
-------------------------------------------------------------------------------

  - Liveness is not automatic and is never inferred.
  - Passive WebSocket connections are not guaranteed to remain healthy.
  - Observability alone does not imply liveness.
  - The Connection enforces health invariants.
  - Protocols are responsible for producing liveness signals.
  - Early warning enables reactive, just-in-time protocol behavior.
  - Forced reconnects are intentional, observable, and recoverable.

This example intentionally makes no assumptions about exchange behavior.
It reports exactly what happens on the wire.

Wirekrak enforces correctness — it does not hide responsibility.
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

inline int run_example(const char* name, const char* url, const char* description, const char* ping_payload = nullptr,
                       int enable_ping_after_failures = 0, std::chrono::seconds observe_for = std::chrono::seconds(10)) {
    using namespace wirekrak::core::transport;
    // WebSocket transport specialization
    using WS = winhttp::WebSocket;

    std::cout << "=== Wirekrak Connection - Heartbeat-driven Liveness (" << name << ") ===\n\n" << description << "\n\n";

    // -------------------------------------------------------------------------
    // Signal handling (explicit termination)
    // -------------------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------------------
    // Connection setup
    // -------------------------------------------------------------------------
    telemetry::Connection telemetry;
    Connection<WS> connection(telemetry);

    std::atomic<uint64_t> forwarded{0};
    std::atomic<int> disconnects{0};
    std::atomic<bool> ping_enabled{false};
    std::atomic<bool> connected{false};

    // -------------------------------------------------------------------------
    // Lambda to drain events
    // -------------------------------------------------------------------------
    auto drain_events = [&]() {
        connection::Signal sig;

        while (connection.poll_signal(sig)) {
            switch (sig) {
                case connection::Signal::Connected:
                    connected.store(true, std::memory_order_relaxed);
                    std::cout << "[example] Connect to " << name << " observed!\n";
                    break;

                case connection::Signal::Disconnected:
                    connected.store(false, std::memory_order_relaxed);
                    std::cout << "[example] Disconnect from " << name << " observed! (exactly once)\n";
                    {
                        const int d = ++disconnects;
                        if (d >= enable_ping_after_failures) {
                            ping_enabled.store(true);
                        }
                    }
                    break;

                case connection::Signal::RetryScheduled:
                    std::cout << "[example] Retry schedule observed!\n";
                    break;
        
                case connection::Signal::LivenessThreatened:
                    std::cout << "[example] Liveness warning observed!\n";
                    if (!ping_enabled.load(std::memory_order_relaxed)) return;
                    if (!connected.load(std::memory_order_relaxed)) return;
                    if (!ping_payload) return;
                    std::cout << "[example] Liveness warning -> sending protocol ping\n";
                    (void)connection.send(ping_payload);
                    break;
                default:
                    break;
            }
        }
    };

    // -------------------------------------------------------------------------
    // Open connection
    // -------------------------------------------------------------------------
    if (connection.open(url) != Error::None) {
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

    while (!ping_enabled.load(std::memory_order_relaxed) && running.load(std::memory_order_relaxed)) {
        connection.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Phase C - protocol-managed heartbeat
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase C - protocol-managed heartbeat\n";

    auto on_liveness_warning = [&](std::chrono::milliseconds remaining) {
        
    };

    while (running.load(std::memory_order_relaxed)) {
        connection.poll();  // Poll driven progression
        drain_events();     // Drain any pending events
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Close connection
    // -------------------------------------------------------------------------
    connection.close();

    for (int i = 0; i < 20; ++i) {
        connection.poll();  // Poll driven progression
        drain_events();     // Drain any pending events
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
