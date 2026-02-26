/*
===============================================================================
Example 4 - Heartbeat & Liveness Responsibility
(Learning Step 5: Health is enforced, not assumed)
===============================================================================

This example demonstrates Wirekrak’s liveness model and the strict separation
of responsibilities between the Connection layer and protocol logic.

After learning:
  - Example 0 → Minimal lifecycle & polling
  - Example 1 → Wire-level message reality
  - Example 2 → Observation vs consumption
  - Example 3 → Failure, disconnect & close ordering

This final step teaches:

> **Liveness is not automatic. It is enforced.**
> **Health must be maintained by the protocol.**

Wirekrak does not guess health.
It measures observable traffic and enforces invariants deterministically.

-------------------------------------------------------------------------------
Core responsibility split
-------------------------------------------------------------------------------

Connection enforces liveness:
  - Requires observable traffic (messages or heartbeats)
  - Emits LivenessThreatened before expiration
  - Force-closes deterministically if silence continues
  - Schedules reconnect according to retry policy

Protocol maintains liveness:
  - Reacts to LivenessThreatened signals
  - Emits protocol-specific pings or heartbeats
  - Decides if and when to respond
  - Never relies on implicit transport behavior

The Connection never invents traffic.
The Protocol never bypasses enforcement.

-------------------------------------------------------------------------------
Execution phases
-------------------------------------------------------------------------------

Phase A - Passive silence
  - A WebSocket connection is opened.
  - No subscriptions or pings are sent.
  - Once traffic ceases within the configured window,
    the Connection emits LivenessThreatened.
  - Continued silence leads to forced reconnect.

Phase B - Protocol-managed heartbeat
  - The protocol reacts to LivenessThreatened.
  - A ping payload is sent explicitly.
  - Observable traffic resumes.
  - Forced reconnects are avoided.

Nothing is inferred.
Nothing is hidden.
Only observable traffic resets liveness.

-------------------------------------------------------------------------------
Key lessons
-------------------------------------------------------------------------------

  - Liveness is never inferred.
  - Silence is treated as failure.
  - Warnings precede expiration.
  - Reconnects are intentional and observable.
  - Protocol logic is responsible for producing health signals.
  - Data-plane consumption does not imply liveness.
  - Enforcement and maintenance are strictly separated.

-------------------------------------------------------------------------------
Key invariant
-------------------------------------------------------------------------------

No observable traffic → warning → expiration → reconnect.

If traffic resumes before expiration,
the connection remains stable.

-------------------------------------------------------------------------------
Learning path position
-------------------------------------------------------------------------------

Example 0 → Minimal lifecycle  
Example 1 → Wire-level reality  
Example 2 → Observation vs consumption  
Example 3 → Failure ordering  
Example 4 → Liveness enforcement & protocol responsibility  

-------------------------------------------------------------------------------
Key takeaway
-------------------------------------------------------------------------------

> Transport enforces.
> Protocol maintains.
> Application observes.

Wirekrak enforces correctness.
It does not hide responsibility.

===============================================================================
*/

#pragma once

#include <iostream>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>
#include <csignal>

#include "wirekrak/core/preset/transport/connection_default.hpp"


// -----------------------------------------------------------------------------
// Ctrl+C handling
// -----------------------------------------------------------------------------
static std::atomic<bool> running{true};

inline void on_signal(int) {
    running.store(false);
}

// -----------------------------------------------------------------------------
// Setup environment
// -----------------------------------------------------------------------------
using namespace wirekrak::core;
using namespace wirekrak::core::transport;

static preset::DefaultMessageRing g_ring;   // Golbal SPSC ring buffer (transport → session)


// -----------------------------------------------------------------------------
// Runner
// -----------------------------------------------------------------------------
inline int run_example(const char* name, const char* url, const char* description, const char* ping_payload = nullptr, int enable_ping_after_failures = 0) {

    std::cout << "=== Wirekrak Connection - Heartbeat-driven Liveness (" << name << ") ===\n\n" << description << "\n" << std::endl;

    // -------------------------------------------------------------------------
    // Signal handling (explicit termination)
    // -------------------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------------------
    // Connection setup
    // -------------------------------------------------------------------------
    telemetry::Connection telemetry;
    preset::transport::DefaultConnection connection(g_ring, telemetry);

    int disconnects{0};
    bool ping_enabled{false};
    bool connected{false};

    // -------------------------------------------------------------------------
    // Lambda to drain events
    // -------------------------------------------------------------------------
    auto drain_signals = [&]() {
        connection::Signal sig;

        while (connection.poll_signal(sig)) {
            switch (sig) {
                case connection::Signal::Connected:
                    connected = true;
                    std::cout << "[example] Connect to " << name << " observed!" << std::endl;
                    break;

                case connection::Signal::Disconnected:
                    connected = false;
                    std::cout << "[example] Disconnect from " << name << " observed! (exactly once)" << std::endl;
                    {
                        const int d = ++disconnects;
                        if (d >= enable_ping_after_failures) {
                            ping_enabled = true;
                        }
                    }
                    break;

                case connection::Signal::RetryImmediate:
                    std::cout << "[example] Immediate retry observed!" << std::endl;
                    break;

                case connection::Signal::RetryScheduled:
                    std::cout << "[example] Retry schedule observed!" << std::endl;
                    break;
        
                case connection::Signal::LivenessThreatened:
                    std::cout << "[example] Liveness warning observed!" << std::endl;
                    if (connected && ping_enabled && ping_payload) {
                        std::cout << "[example] Liveness warning -> sending protocol ping" << std::endl;
                        (void)connection.send(ping_payload);
                    }
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
    std::cout << "\n[example] Phase A - passive observation" << std::endl;

    auto start = std::chrono::steady_clock::now();
    while (!ping_enabled && running.load(std::memory_order_relaxed)) {
        connection.poll();   // Poll-driven execution
        drain_signals();     // Drain any pending signals
        std::this_thread::yield();
    }

    // -------------------------------------------------------------------------
    // Phase B - protocol-managed heartbeat
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase B - protocol-managed heartbeat" << std::endl;

    while (running.load(std::memory_order_relaxed)) {
        connection.poll();   // Poll-driven execution
        drain_signals();     // Drain any pending signals
        // Pull data-plane messages (explicit consumption)
        while (auto* block = connection.peek_message()) {
            std::cout << "[example] RX message (" << block->size << " bytes)" << std::endl;
            connection.release_message();
        }
        std::this_thread::yield();
    }

    // -------------------------------------------------------------------------
    // Close connection
    // -------------------------------------------------------------------------
    connection.close();

    // Drain remaining events until idle
    while (!connection.is_idle()) {
        connection.poll();   // Poll-driven execution
        drain_signals();     // Drain any pending signals
        std::this_thread::yield();
    }

    // -------------------------------------------------------------------------
    // Dump telemetry
    // -------------------------------------------------------------------------
    connection.telemetry().debug_dump(std::cout);

    // -------------------------------------------------------------------------
    // Interpretation guide
    // -------------------------------------------------------------------------
    std::cout << "\n=== Key observations ===\n"
        << "- Passive connections may fail liveness.\n"
        << "- Forced reconnects are intentional and observable.\n"
        << "- Protocol pings restore liveness stability.\n"
        << "- Connection enforces health; protocol provides signals.\n\n"
        << "Wirekrak reports reality - it does not hide responsibility." << std::endl;

    return 0;
}
