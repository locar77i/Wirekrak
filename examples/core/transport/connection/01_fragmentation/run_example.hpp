/*
===============================================================================
Example 1 - Message Shape & Fragmentation
(Learning Step 2: Observing the wire)
===============================================================================

This example is the **second learning step after Example 0 (Minimal Connection)**.

After learning how to:
  - open a connection
  - poll the connection
  - pull messages from the data-plane
  - close cleanly

This example teaches a deeper truth:

> **What you pull is not what was sent.**
> **What you observe is not what was intended.**

Wirekrak reports **observable wire reality**, not sender intent.

-------------------------------------------------------------------------------
What this example demonstrates
-------------------------------------------------------------------------------

It shows how Wirekrak reports **message shape** based on actual WebSocket
framing behavior:

  - how messages are split into frames
  - how frames are reassembled
  - how sizes are measured
  - how fragmentation is detected

All telemetry is derived from **wire behavior**, not protocol semantics.

This example intentionally pulls all available messages in order to observe
their reconstructed size and fragmentation characteristics.

-------------------------------------------------------------------------------
Scenario
-------------------------------------------------------------------------------

  1) Connect to a WebSocket endpoint
  2) Send a subscription message
  3) Pull messages from the connection data-plane
  4) Observe framing behavior
  5) Dump transport and connection telemetry

-------------------------------------------------------------------------------
What this teaches
-------------------------------------------------------------------------------

  - Transport RX messages and frames are different concepts
  - “One logical message” does not imply “one frame”
  - Fragmentation is transport-level reality
  - Message size is an observed property, not a protocol promise
  - Pulling (consumption) is separate from transport observation
  - Telemetry reflects **wire mechanics**, not protocol meaning

-------------------------------------------------------------------------------
Learning path position
-------------------------------------------------------------------------------

Example 0 → How to connect and poll  
Example 1 → How to observe wire reality (this example)  
Example 2 → Observation vs consumption semantics  
Example 3 → Error & close lifecycle correctness  
Example 4 → Liveness and protocol responsibility  

-------------------------------------------------------------------------------
Key takeaway
-------------------------------------------------------------------------------

> Wirekrak does not model intent.
> Wirekrak models reality.

Transport exposes facts.
Connection exposes availability.
Applications explicitly consume what they pull.

Telemetry reflects **what happened on the wire**, not what was meant.

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

    std::cout << "=== Wirekrak Connection - Message Shape & Fragmentation (" << name << ") ===\n\n" << description << "\n" << std::endl;

    // -------------------------------------------------------------------------
    // Signal handling (explicit termination)
    // -------------------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------------------
    // Connection setup
    // -------------------------------------------------------------------------
    telemetry::Connection telemetry;
    Connection<WS> connection(telemetry);
    bool disconnected = false;

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
                    disconnected = true;
                    break;

                case connection::Signal::RetryImmediate:
                    std::cout << "[example] Immediate retry observed!" << std::endl;
                    break;

                case connection::Signal::RetryScheduled:
                    std::cout << "[example] Retry schedule observed!" << std::endl;
                    break;

                case connection::Signal::LivenessThreatened:
                    std::cout << "[example] Liveness warning observed!" << std::endl;
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
    // Subscribe
    // -------------------------------------------------------------------------
    (void)connection.send(subscribe_payload);

    // -------------------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // -------------------------------------------------------------------------
    int idle_spins = 0;
    bool did_work = false;
    while (running.load(std::memory_order_relaxed) && !disconnected) {
        connection.poll();  // Poll-driven execution
        drain_signals();    // Drain any pending signals
        // Pull data-plane messages explicitly
        while (auto* block = connection.peek_message()) {
            std::cout << "[example] RX message (" << block->size << " bytes)" << std::endl;
            // Uncomment to inspect raw payload
            // std::cout.write(block->data, block->size);
            // std::cout << "" << std::endl;
            connection.release_message();
            did_work = true;
        }
        if (did_work) {
            idle_spins = 0;
            did_work = false;
        }
        else if (++idle_spins > 100) {
            std::this_thread::yield();
            idle_spins = 0;
        }
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
    std::cout << "\n=== How to read this ===\n\n"

        << "This example is about observing reality on the wire.\n"
        << "The numbers describe how data actually moved,\n"
        << "not what the exchange intended.\n\n"

        << "Start with [WebSocket RX messages]:\n"
        << "  These are fully reassembled messages observed at the transport layer.\n"
        << "  They reflect what arrived on the wire.\n\n"

        << "Next, look at [Messages forwarded]:\n"
        << "  This increments only when the application calls peek_message().\n"
        << "  It reflects explicit consumption of available messages.\n\n"

        << "Then examine [Fragments/msg]:\n"
        << "  A value of 1 means a message arrived in a single frame.\n"
        << "  Values greater than 1 indicate transport-level fragmentation.\n\n"

        << "Finally, check [RX fragments (total)]:\n"
        << "  This is the total number of frames observed on the wire.\n\n"

        << "Key insight:\n"
        << "  One logical message may span multiple frames,\n"
        << "  and consumption is separate from observation.\n\n"

        << "Wirekrak does not guess or normalize.\n"
        << "It exposes facts, availability, and explicit consumption."
        << std::endl;

    return 0;
}
