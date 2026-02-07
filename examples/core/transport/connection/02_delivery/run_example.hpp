#pragma once

/*
===============================================================================
Example 2 - Connection vs Transport Semantics
(Learning Step 3: Observation ≠ Delivery)
===============================================================================

This example is the **third learning step** in the Wirekrak connection model.

After learning how to:
  - open and run a connection        (Example 0)
  - observe wire-level message shape (Example 1)

This example teaches a critical distinction:

> **Receiving data is not the same as delivering data.**

Wirekrak intentionally separates:
  - what arrives on the wire
  - from what is delivered to user code

-------------------------------------------------------------------------------
What this example demonstrates
-------------------------------------------------------------------------------

This example shows the semantic boundary between:

  • WebSocket transport
      → What physically arrives from the network

  • Connection layer
      → What is intentionally forwarded to the application

It proves that:

> **messages_rx_total != messages_forwarded_total is correct behavior**

-------------------------------------------------------------------------------
Scenario
-------------------------------------------------------------------------------

The example runs in two explicit phases:

Phase A - Transport-only observation
  - A connection is opened
  - A subscription is sent
  - NO message callback is installed
  - The transport receives messages
  - The connection forwards NOTHING to user code

Phase B - Explicit delivery opt-in
  - A message callback is installed
  - The same incoming messages are now forwarded
  - Delivery becomes visible to the application

Nothing else changes - only intent.

-------------------------------------------------------------------------------
What this teaches
-------------------------------------------------------------------------------

  - Receiving ≠ delivering
  - Observation does not imply consumption
  - Applications must opt-in to message delivery
  - Telemetry distinguishes transport activity from application handoff
  - Silent dropping is not a bug - it is policy

-------------------------------------------------------------------------------
Learning path position
-------------------------------------------------------------------------------

Example 0 → How to connect and poll  
Example 1 → How to observe wire reality  
Example 2 → Why observation ≠ delivery  
Example 3 → Error & close lifecycle correctness  
Example 4 → Liveness and protocol responsibility  

-------------------------------------------------------------------------------
Key takeaway
-------------------------------------------------------------------------------

> Transport reports what happened.
> Connection decides what matters.
> Applications receive only what they ask for.

Wirekrak separates **fact**, **policy**, and **intent** - on purpose.

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

inline int run_example(const char* name, const char* url, const char* description, const char* subscribe_payload) {
    using namespace wirekrak::core::transport;
    // WebSocket transport specialization
    using WS = winhttp::WebSocket;

    std::cout << "=== Wirekrak Connection - Transport vs Delivery (" << name << ") ===\n\n" << description << "\n\n"; 

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
    auto drain_events = [&]() {
        connection::Signal sig;

        while (connection.poll_signal(sig)) {
            switch (sig) {
                case connection::Signal::Connected:
                    std::cout << "[example] Connect to " << name << " observed!\n";
                    break;

                case connection::Signal::Disconnected:
                    std::cout << "[example] Disconnect from " << name << " observed!\n";
                    break;

                case connection::Signal::RetryScheduled:
                    std::cout << "[example] Retry schedule observed!\n";
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
    // Phase A - no message hook
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase A - transport receives, connection does NOT forward\n";

    auto phase_a_start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - phase_a_start < std::chrono::seconds(10) && running.load(std::memory_order_relaxed)) {
        connection.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Phase B - install message hook
    // -------------------------------------------------------------------------
    std::cout << "\n[example] Phase B - installing message callback\n";

    std::atomic<uint64_t> forwarded{0};
    connection.on_message([&](std::string_view msg) {
        ++forwarded;
        std::cout << "[example] Forwarded message (" << msg.size() << " bytes)\n";
    });

    while (running.load(std::memory_order_relaxed)) {
        connection.poll();  // Poll driven progression
        drain_events();     // Drain any pending events
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Close connection
    // -------------------------------------------------------------------------
    connection.close();

    // Drain remaining events (~200 ms)
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
    // How to read this
    // -------------------------------------------------------------------------
    std::cout << "\n=== key insights ===\n"
        << "[RX messages]---------  observed arriving messages on the wire, regardless of application interest.\n"
        << "[Messages forwarded]--  messages intentionally delivered to user code. Only counted after a message callback exists.\n"
        << "It is expected and correct that:  [Messages forwarded] <= [RX messages]\n\n"
        << "Wirekrak separates observation (transport) from policy and delivery (connection).\n";

    return 0;
}
