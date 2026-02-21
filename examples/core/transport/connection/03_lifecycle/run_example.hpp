/*
===============================================================================
Example 3 - Failure, Disconnect & Close Ordering
(Learning Step 4: Deterministic correctness under failure)
===============================================================================

This example is the **fourth learning step** in the Wirekrak connection model.

After learning how to:
  - open and run a connection            (Example 0)
  - observe wire-level message reality   (Example 1)
  - separate observation from consumption (Example 2)

This example teaches the hardest rule in networking systems:

> **Failure must be observable, ordered, and unambiguous.**

Wirekrak treats errors, disconnects, and closure as **first-class lifecycle facts** —
not side effects, not logs, and not guesses.

-------------------------------------------------------------------------------
Core idea
-------------------------------------------------------------------------------

In real systems, failures are messy:

  - Errors occur while receiving
  - Connections close locally or remotely
  - Retries are scheduled
  - Resources are torn down
  - Messages may still be in flight

Most systems blur these events together.

Wirekrak does not.

It enforces **strict lifecycle invariants** so that:
  - Every failure has a cause
  - Logical disconnect happens exactly once
  - Physical close is idempotent
  - Retry follows disconnect — never precedes it
  - All transitions are observable and ordered

-------------------------------------------------------------------------------
What this example demonstrates
-------------------------------------------------------------------------------

This example validates how Wirekrak handles:

  • Transport-level errors  
  • Logical connection shutdown  
  • Physical WebSocket closure  
  • Retry scheduling  
  • Explicit data-plane consumption  

And, most importantly:

> **How these events are ordered, counted, and verified.**

-------------------------------------------------------------------------------
Scenario
-------------------------------------------------------------------------------

  1) Connect to a WebSocket endpoint
  2) Optionally send a raw payload (protocol-agnostic)
  3) Allow a transport error or remote close to occur
  4) Explicitly pull data-plane messages (peek_message / release_message)
  5) Observe connection::Signal ordering
  6) Trigger a local close()
  7) Drain until idle
  8) Dump connection and transport telemetry

The goal is not to avoid failure —
but to **observe it correctly and deterministically**.

-------------------------------------------------------------------------------
What this teaches
-------------------------------------------------------------------------------

  - Errors may occur before closure
  - Errors do NOT replace disconnect
  - Logical disconnect is emitted exactly once
  - Physical close events are idempotent
  - Retry is scheduled only after disconnect
  - Lifecycle events are never double-counted
  - Local close() is idempotent and safe
  - Data-plane consumption does not interfere with lifecycle correctness

-------------------------------------------------------------------------------
Key invariants validated
-------------------------------------------------------------------------------

  - Error → then Disconnect (never reversed)
  - Disconnect signal is emitted exactly once
  - Physical close is counted exactly once
  - Retry follows real failure cause
  - Telemetry reflects observable reality

If any of these invariants break,
the system becomes untrustworthy.

-------------------------------------------------------------------------------
Learning path position
-------------------------------------------------------------------------------

Example 0 → Minimal lifecycle & polling  
Example 1 → Message shape & fragmentation  
Example 2 → Observation vs consumption  
Example 3 → Failure, disconnect & close ordering  
Example 4 → Liveness and protocol responsibility  

-------------------------------------------------------------------------------
Key takeaway
-------------------------------------------------------------------------------

> Errors may happen.
> Disconnect must be singular.
> Closure must be exact.
> Ordering must be deterministic.

Wirekrak does not hide failure.

It models failure **precisely and observably**.

===============================================================================
*/

#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#include "wirekrak/core.hpp"

// -----------------------------------------------------------------------------
// Setup environment
// -----------------------------------------------------------------------------
using namespace wirekrak::core;
using namespace wirekrak::core::transport;

static MessageRingT g_ring;   // Golbal SPSC ring buffer (transport → session)


// -----------------------------------------------------------------------------
// Runner
// -----------------------------------------------------------------------------
inline int run_example(const char* name, const char* url, const char* description, const char* subscribe_payload,
                       std::chrono::seconds runtime = std::chrono::seconds(5)) {

    std::cout << "=== Wirekrak Connection - Error & Close Lifecycle (" << name << ") ===\n\n" << description << "\n" << std::endl;
  
    // -------------------------------------------------------------------------
    // Connection setup
    // -------------------------------------------------------------------------
    telemetry::Connection telemetry;
    ConnectionT connection(g_ring, telemetry);

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
                    std::cout << "[example] Disconnect from " << name << " observed! (exactly once)" << std::endl;
                    break;

                case connection::Signal::RetryImmediate:
                    std::cout << "[example] Immediate retry observed!" << std::endl;
                    break;

                case connection::Signal::RetryScheduled:
                    std::cout << "[example] Retry schedule observed!" << std::endl;
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
        connection.poll();   // Poll-driven execution
        drain_signals();     // Drain any pending signals
        // Pull data-plane messages explicitly
        while (auto* block = connection.peek_message()) {
            std::cout << "[example] RX message (" << block->size << " bytes)" << std::endl;
            connection.release_message();
        }
        std::this_thread::yield();
    }

    // ---------------------------------------------------------------------
    // Force local shutdown (idempotent)
    // ---------------------------------------------------------------------
    connection.close();

    // Drain remaining events until idle
    while (!connection.is_idle()) {
        connection.poll();   // Poll-driven execution
        drain_signals();     // Drain any pending signals
        std::this_thread::yield();
    }

    // ---------------------------------------------------------------------
    // Dump telemetry
    // ---------------------------------------------------------------------
    std::cout << "\n=== Connection Telemetry ===" << std::endl;
    telemetry.debug_dump(std::cout);

    std::cout << "\n=== WebSocket Telemetry ===" << std::endl;
    telemetry.websocket.debug_dump(std::cout);

    // ---------------------------------------------------------------------
    // Interpretation guide
    // ---------------------------------------------------------------------

    std::cout << "\n=== How to read this ===\n\n"
        << "This example validates lifecycle correctness under failure.\n\n"

        << "Event ordering must always follow logical causality:\n\n"

        << "1) Transport error (optional)\n"
        << "2) Logical Disconnected (exactly once)\n"
        << "3) Physical WebSocket close\n"
        << "4) Retry scheduling (if retryable)\n\n"

        << "Inspect telemetry carefully:\n\n"

        << "Connection telemetry:\n"
        << "  Disconnect events → must be exactly 1 per shutdown.\n\n"

        << "WebSocket telemetry:\n"
        << "  Receive errors → explain WHY failure occurred.\n"
        << "  Close events   → physical socket closure.\n\n"

        << "Invariant summary:\n"
        << "  Errors may happen.\n"
        << "  Disconnect happens once.\n"
        << "  Close happens once.\n"
        << "  Retry follows cause.\n\n"

        << "If ordering or counts ever disagree,\n"
        << "the system is lying.\n\n"

        << "Wirekrak guarantees ordered, observable failure."
        << std::endl;

    return 0;
}
