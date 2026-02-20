/*
===============================================================================
Example 2 - Connection vs Transport Semantics
(Learning Step 3: Observation ≠ Consumption)
===============================================================================

This example is the **third learning step** in the Wirekrak connection model.

After learning how to:
  - open and poll a connection       (Example 0)
  - observe wire-level message shape (Example 1)

This example teaches a critical distinction:

> **Receiving data is not the same as consuming data.**

Wirekrak intentionally separates:

  - what arrives on the wire
  - from what the application explicitly pulls

There is no automatic delivery.
There are no callbacks.
There is no implicit consumption.

Delivery occurs only when the application calls:

    peek_message() + release_message()

-------------------------------------------------------------------------------
What this example demonstrates
-------------------------------------------------------------------------------

This example shows the semantic boundary between:

  • WebSocket transport
      → What physically arrives from the network
      → Counted as RX messages

  • Connection data-plane
      → Messages made available by the transport
      → Counted as forwarded only when the application pulls them

It proves that:

> **RX messages != messages_forwarded_total is correct behavior**

-------------------------------------------------------------------------------
Scenario
-------------------------------------------------------------------------------

The example runs in two explicit phases:

Phase A - Transport-only observation
  - A connection is opened
  - A subscription is sent
  - The application DOES NOT call peek_message()
  - The transport receives messages
  - Nothing is consumed

Phase B - Explicit consumption
  - The application begins calling peek_message()
  - Messages are now observed and released
  - Forwarded counter increases

Nothing else changes - only application behavior.

-------------------------------------------------------------------------------
What this teaches
-------------------------------------------------------------------------------

  - Receiving ≠ consuming
  - Observation requires explicit pull
  - Applications must actively drain the data-plane
  - Telemetry distinguishes transport activity from consumption
  - Lack of consumption is not a bug - it is policy

-------------------------------------------------------------------------------
Learning path position
-------------------------------------------------------------------------------

Example 0 → How to connect and poll  
Example 1 → How to observe wire reality  
Example 2 → Why observation ≠ consumption  
Example 3 → Error & close lifecycle correctness  
Example 4 → Liveness and protocol responsibility  

-------------------------------------------------------------------------------
Key takeaway
-------------------------------------------------------------------------------

> Transport reports what happened.
> Connection exposes what is available.
> Applications consume only what they explicitly pull.

Wirekrak separates **fact**, **availability**, and **consumption** - on purpose.

===============================================================================
*/

#pragma once

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

inline int run_example(const char* name, const char* url, const char* description, const char* subscribe_payload) {
    using namespace wirekrak::core::transport;
    // WebSocket transport specialization
    using WS = winhttp::WebSocket;

    std::cout << "=== Wirekrak Connection - Observation vs Consumption (" << name << ") ===\n\n" << description << "\n" << std::endl;

    // -------------------------------------------------------------------------
    // Signal handling (explicit termination)
    // -------------------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------------------
    // Connection setup
    // -------------------------------------------------------------------------
    telemetry::Connection telemetry;
    Connection<WS> connection(telemetry);

    // -------------------------------------------------------------------------
    // Lambda to drain events
    // -------------------------------------------------------------------------
    auto drain_signals = [&]() {
        connection::Signal sig;

        while (connection.poll_signal(sig)) {
            switch (sig) {
                case connection::Signal::Connected:
                    std::cout << "[example] Connect to " << name << " observed!" << std::endl;
                    break;

                case connection::Signal::Disconnected:
                    std::cout << "[example] Disconnect from " << name << " observed!" << std::endl;
                    break;

                case connection::Signal::RetryImmediate:
                    std::cout << "[example] Immediate retry observed!" << std::endl;
                    break;

                case connection::Signal::RetryScheduled:
                    std::cout << "[example] Retry schedule observed!" << std::endl;
                    break;

                case connection::Signal::LivenessThreatened:
                    std::cout << "[example] Liveness threatened observed!" << std::endl;
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
    // Subscribe immediately
    // -------------------------------------------------------------------------
    (void)connection.send(subscribe_payload);

    // -------------------------------------------------------------------------
    // Phase A - Do NOT pull messages
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase A - transport receives, application does NOT pull" << std::endl;

    auto phase_a_start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - phase_a_start < std::chrono::seconds(10) && running.load(std::memory_order_relaxed)) {
        connection.poll();    //Poll-driven execution
        drain_signals();      // signals drained
        // No peek_message() here
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Phase B - Explicit delivery (pull messages)
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase B - application begins pulling messages" << std::endl;

    while (running.load(std::memory_order_relaxed)) {
        connection.poll();   // Poll-driven execution
        drain_signals();     // Drain any pending signals
        // Pull data-plane messages explicitly
        while (auto* block = connection.peek_message()) {
            std::cout << "[example] Delivered message (" << block->size << " bytes)" << std::endl;
            connection.release_message();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Close connection
    // -------------------------------------------------------------------------
    connection.close();

    // Drain remaining events until idle
    while (!connection.is_idle()) {
        connection.poll();   // Poll-driven execution
        drain_signals();     // Drain any pending signals
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Dump telemetry
    // -------------------------------------------------------------------------
    std::cout << "\n=== Connection Telemetry ===" << std::endl;
    telemetry.debug_dump(std::cout);

    std::cout << "\n=== WebSocket Telemetry ===" << std::endl;
    telemetry.websocket.debug_dump(std::cout);

    // -------------------------------------------------------------------------
    // How to read this
    // -------------------------------------------------------------------------
    std::cout << "\n=== Key Insights ===\n\n"
              << "[RX messages] -------- observed arriving messages on the wire.\n"
              << "[Messages forwarded] -- incremented only when peek_message() is called.\n\n"
              << "It is expected and correct that:\n"
              << "  Messages forwarded <= RX messages\n\n"
              << "Transport reports what happened.\n"
              << "Connection exposes what is pulled.\n"
              << "Applications receive only what they explicitly consume." << std::endl;

    return 0;
}
