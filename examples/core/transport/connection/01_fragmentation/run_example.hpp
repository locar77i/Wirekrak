#pragma once

/*
===============================================================================
Example 1 - Message Shape & Fragmentation
(Learning Step 2: Observing the wire)
===============================================================================

This example is the **second learning step after Example 0 (Minimal Connection)**.

After learning how to:
  - open a connection
  - poll the connection
  - receive messages
  - close cleanly

This example teaches a deeper truth:

> **What you receive is not what was sent.**
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

-------------------------------------------------------------------------------
Scenario
-------------------------------------------------------------------------------

  1) Connect to a WebSocket endpoint
  2) Send a subscription message
  3) Receive messages of varying sizes
  4) Observe framing behavior
  5) Dump transport and connection telemetry

-------------------------------------------------------------------------------
What this teaches
-------------------------------------------------------------------------------

  - RX messages and frames are different concepts
  - “One message” does not imply “one frame”
  - Fragmentation is transport-level reality
  - Message size is an observed property, not a protocol promise
  - Telemetry reflects **delivery mechanics**, not protocol meaning

-------------------------------------------------------------------------------
Learning path position
-------------------------------------------------------------------------------

Example 0 → How to connect and run a connection  
Example 1 → How to observe wire reality (this example)  
Example 2 → Transport vs delivery semantics  
Example 3 → Error & close lifecycle correctness  
Example 4 → Liveness and protocol responsibility  

-------------------------------------------------------------------------------
Key takeaway
-------------------------------------------------------------------------------

> Wirekrak does not model intent.  
> Wirekrak models reality.

Telemetry reflects **what happened**, not what was meant.

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

    std::cout << "=== Wirekrak Connection - Message Shape & Fragmentation (" << name << ") ===\n\n" << description << "\n\n";

    // -------------------------------------------------------------------------
    // Signal handling (explicit termination)
    // -------------------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------------------
    // Connection setup
    // -------------------------------------------------------------------------
    telemetry::Connection telemetry;
    Connection<WS> connection(telemetry);

    connection.on_message([&](std::string_view msg) {
        std::cout << "[example] RX message (" << msg.size() << " bytes)\n";
        // Uncomment for raw payload inspection:
        //std::cout << msg << "\n";
    });

    // -------------------------------------------------------------------------
    // Lambda to drain events
    // -------------------------------------------------------------------------
    auto drain_events = [&]() {
        TransitionEvent ev;

        while (connection.poll_event(ev)) {
            switch (ev) {
                case TransitionEvent::Connected:
                    std::cout << "[example] Connect to " << name << " observed!\n";
                    break;

                case TransitionEvent::Disconnected:
                    std::cout << "[example] Disconnect from " << name << " observed!\n";
                    break;

                case TransitionEvent::RetryScheduled:
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
    // Subscribe
    // -------------------------------------------------------------------------
    (void)connection.send(subscribe_payload);

    // -------------------------------------------------------------------------
    // Main polling loop (runs until Ctrl+C)
    // -------------------------------------------------------------------------
    while (running.load(std::memory_order_relaxed)) {
        connection.poll(); // Poll driven progression
        drain_events();    // Drain any pending events
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Close connection
    // -------------------------------------------------------------------------
    connection.close();

    // Drain any remaining messages (~200 ms)
    for (int i = 0; i < 20; ++i) {
        connection.poll(); // Poll driven progression
        drain_events();    // Drain any pending events
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
    std::cout << "\n=== How to read this ===\n\n"

          << "This example is about observing reality on the wire.\n"
          << "The numbers you see describe how data actually moved,\n"
          << "not what the exchange or protocol intended.\n\n"

          << "Start with [RX messages]:\n"
          << "  This is the number of complete messages delivered to user code.\n"
          << "  Each one represents a fully reassembled WebSocket message.\n\n"

          << "Next, look at [RX message bytes]:\n"
          << "  This shows the distribution of message sizes after reassembly.\n"
          << "  These sizes are observed facts, not promised payload sizes.\n\n"

          << "Then examine [Fragments/msg]:\n"
          << "  A value of 1 means messages arrived in a single frame.\n"
          << "  Values greater than 1 mean the message was fragmented\n"
          << "  across multiple WebSocket frames on the wire.\n\n"

          << "Finally, check [RX fragments (total)]:\n"
          << "  This is the total number of frames observed.\n"
          << "  It tells you how much framing work the transport performed.\n\n"

          << "Key insight:\n"
          << "  A single logical message may arrive in multiple frames,\n"
          << "  and Wirekrak reports that distinction explicitly.\n\n"

          << "Wirekrak does not guess, normalize, or reinterpret.\n"
          << "It reports exactly what happened on the wire.\n";

    return 0;
}
