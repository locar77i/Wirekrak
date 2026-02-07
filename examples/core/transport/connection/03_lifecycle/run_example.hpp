#pragma once

/*
===============================================================================
Example 3 - Error & Close Lifecycle
(Learning Step 4: Correctness under failure)
===============================================================================

This example is the **fourth learning step** in the Wirekrak connection model.

After learning how to:
  - open and run a connection            (Example 0)
  - observe wire-level message reality   (Example 1)
  - separate observation from delivery   (Example 2)

This example teaches the hardest rule in networking systems:

> **Failure must be observable, ordered, and unambiguous.**

Wirekrak treats errors and closure as **first-class events** -
not side effects, not logs, and not guesses.

-------------------------------------------------------------------------------
Core idea
-------------------------------------------------------------------------------

In real systems, failures are messy:

  - Errors occur while receiving
  - Connections close locally or remotely
  - Retries are scheduled
  - Resources are torn down

Most systems blur these events together.

Wirekrak does not.

It enforces **strict lifecycle invariants** so that:
  - Every failure has a cause
  - Every close happens exactly once
  - Every event is observable and ordered

-------------------------------------------------------------------------------
What this example demonstrates
-------------------------------------------------------------------------------

This example validates how Wirekrak handles:

  • Transport-level errors  
  • Logical connection shutdown  
  • Physical WebSocket closure  
  • Retry scheduling  

And, most importantly:

> **How these events are ordered and counted.**

-------------------------------------------------------------------------------
Scenario
-------------------------------------------------------------------------------

  1) Connect to a WebSocket endpoint
  2) Optionally send a raw payload (protocol-agnostic)
  3) Allow a transport error or remote close to occur
  4) Observe connection::Signal ordering
  5) Trigger a local close()
  6) Dump connection and transport telemetry

The goal is not to avoid failure -
but to **observe it correctly**.

-------------------------------------------------------------------------------
What this teaches
-------------------------------------------------------------------------------

  - Errors may occur before closure
  - Errors do NOT replace closure
  - Close is observed exactly once
  - Retry is scheduled after failure
  - Lifecycle events are never double-counted
  - Local close() is idempotent and safe

-------------------------------------------------------------------------------
Key invariants validated
-------------------------------------------------------------------------------

  - Error → then Close (never reversed)
  - Disconnected signal is emitted exactly once
  - Transport close events are idempotent
  - Retry logic observes real failure causes
  - Telemetry reflects reality, not assumptions

If any of these invariants break,
the system becomes untrustworthy.

-------------------------------------------------------------------------------
Learning path position
-------------------------------------------------------------------------------

Example 0 → How to connect and poll  
Example 1 → How to observe wire reality  
Example 2 → Why observation ≠ delivery  
Example 3 → Failure, error, and close correctness  
Example 4 → Liveness and protocol responsibility  

-------------------------------------------------------------------------------
Key takeaway
-------------------------------------------------------------------------------

> Errors may happen.
> Closures must be exact.
> Ordering must be deterministic.

Wirekrak does not hide failure.

It **models it precisely**.

===============================================================================
*/


#include <iostream>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>

#include "wirekrak/core/transport/connection.hpp"
#include "wirekrak/core/transport/winhttp/websocket.hpp"


inline int run_example(const char* name, const char* url, const char* description, const char* subscribe_payload,
                       std::chrono::seconds runtime = std::chrono::seconds(5)) {
    using namespace wirekrak::core::transport;
    // WebSocket transport specialization
    using WS = winhttp::WebSocket;

    std::cout << "=== Wirekrak Connection - Error & Close Lifecycle (" << name << ") ===\n\n" << description << "\n\n";
  
    // -------------------------------------------------------------------------
    // Connection setup
    // -------------------------------------------------------------------------
    wirekrak::core::transport::telemetry::Connection telemetry;
    Connection<WS> connection(telemetry);

    connection.on_message([&](std::string_view msg) {
        std::cout << "[example] RX message (" << msg.size() << " bytes)\n";
    });

    // -------------------------------------------------------------------------
    // Lambda to drain events
    // -------------------------------------------------------------------------
    auto drain_signals = [&]() {
        connection::Signal sig;

        while (connection.poll_signal(sig)) {
            switch (sig) {
                case connection::Signal::Connected:
                    std::cout << "[example] Connect to " << name << " observed!\n";
                    break;

                case connection::Signal::Disconnected:
                    std::cout << "[example] Disconnect from " << name << " observed! (exactly once)\n";
                    break;

                case connection::Signal::RetryScheduled:
                    std::cout << "[example] Retry schedule observed!\n";
                    break;

                // connection::Signal::LivenessThreatened intentionally ignored in this example
                // This example focuses on error/close ordering, not liveness recovery
        
                default:
                    break;
            }
        }
    };
  
    // ---------------------------------------------------------------------
    // Open connection
    // ---------------------------------------------------------------------
    if (connection.open(url) != wirekrak::core::transport::Error::None) {
        return 1;
    }

    // ---------------------------------------------------------------------
    // Send subscription (if any)
    // ---------------------------------------------------------------------
    if (subscribe_payload && subscribe_payload[0] != '\0') {
        (void)connection.send(subscribe_payload);
    }

    // ---------------------------------------------------------------------
    // Observation window
    // ---------------------------------------------------------------------
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < runtime) {
        connection.poll();  // Poll driven progression
        drain_signals();     // Drain any pending signals
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // ---------------------------------------------------------------------
    // Force local shutdown (idempotent)
    // ---------------------------------------------------------------------
    connection.close();

    for (int i = 0; i < 20; ++i) {
        connection.poll();  // Poll driven progression
        drain_signals();     // Drain any pending signals
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

    // ---------------------------------------------------------------------
// How to read this
// ---------------------------------------------------------------------

std::cout << "\n=== How to read this ===\n\n"

        << "This example is about **failure correctness**, not success.\n"
        << "You are expected to see errors, closes, and retries.\n\n"

        << "Start with the order of events printed above.\n"
        << "They must always follow the same logical sequence.\n\n"

        << "1) Errors\n"
        << "   Transport-level errors may occur while receiving data.\n"
        << "   These represent *what went wrong*.\n"
        << "   Errors do NOT close the connection by themselves.\n\n"

        << "2) Disconnect event\n"
        << "   The Connection reports a logical disconnect exactly once.\n"
        << "   This is the moment the connection is considered dead.\n"
        << "   It must never fire twice.\n\n"

        << "3) Close events (WebSocket)\n"
        << "   Physical WebSocket closure may occur after an error\n"
        << "   or as part of a local shutdown.\n"
        << "   Close events are idempotent and never double-counted.\n\n"

        << "4) Retry scheduling\n"
        << "   If the failure is retryable, a retry is scheduled *after* disconnect.\n"
        << "   Retries are driven by cause, not by timing accidents.\n\n"

        << "Now inspect the telemetry below:\n\n"

        << "Connection telemetry:\n"
        << "  Disconnect events\n"
        << "    Must always be exactly 1 per shutdown.\n\n"

        << "WebSocket telemetry:\n"
        << "  Receive errors\n"
        << "    May be zero or more.\n"
        << "    Errors explain *why* the connection closed.\n\n"

        << "  Close events\n"
        << "    Physical socket closures.\n"
        << "    These are expected and may be triggered by errors or by close().\n\n"

        << "Key invariant:\n"
        << "  Errors may happen.\n"
        << "  Close always happens.\n"
        << "  Close happens exactly once.\n\n"

        << "If these counts or orders ever disagree,\n"
        << "the system is lying to you.\n\n"

        << "Wirekrak enforces lifecycle correctness so that\n"
        << "failures are observable, ordered, and debuggable.\n";

    return 0;
}
