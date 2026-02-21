#pragma once

/*
===============================================================================
Example 0 - Minimal Connection Lifecycle
===============================================================================

This is the **onboarding example** for Wirekrak.

It demonstrates the *absolute minimum* required to use a
wirekrak::core::transport::Connection correctly.

No protocol logic.
No subscriptions.
No assumptions about server behavior.

Just:
  - Open
  - Poll
  - Observe lifecycle
  - Close
  - Inspect telemetry

-------------------------------------------------------------------------------
What this example teaches
-------------------------------------------------------------------------------

- How to construct a Connection
- How to open a WebSocket URL
- Why poll() is mandatory
- How lifecycle callbacks behave
- How to shut down cleanly
- Where telemetry comes from

-------------------------------------------------------------------------------
What this example is NOT
-------------------------------------------------------------------------------

- ❌ No subscriptions
- ❌ No message parsing
- ❌ No protocol semantics
- ❌ No liveness tricks
- ❌ No retries demonstrated explicitly

Those come later.

-------------------------------------------------------------------------------
Key takeaway
-------------------------------------------------------------------------------

If you understand this example, you understand:
  • how Wirekrak runs
  • how control flows
  • where responsibility lives

Everything else builds on this.
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

inline int run_example(const char* name, const char* url, const char* description,
                       std::chrono::seconds runtime = std::chrono::seconds(10)) {
    using namespace wirekrak::core::transport;
    // WebSocket transport specialization
    using WS = winhttp::WebSocket;

    std::cout << "=== Wirekrak Connection - Minimal Lifecycle (" << name << ") ===\n\n" << description << "\n" << std::endl;

    // -------------------------------------------------------------------------
    // Signal handling (explicit termination)
    // -------------------------------------------------------------------------
    std::signal(SIGINT, on_signal);

    // -------------------------------------------------------------------------
    // Connection setup
    // -------------------------------------------------------------------------
    // Telemetry is mandatory - it is not optional in Wirekrak.
    telemetry::Connection telemetry;

    // Connection owns the logical lifecycle, retries, and liveness.
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
                    std::cout << "[example] Liveness warning observed!" << std::endl;
                    break;
        
                default:
                    break;
            }
        }
    };

    // Note:
    // No data-plane consumption is performed on purpose.
    // This example focuses on lifecycle, not message handling.

    // -------------------------------------------------------------------------
    // Open connection
    // -------------------------------------------------------------------------
    if (connection.open(url) != Error::None) {
        return 1;
    }

    // -------------------------------------------------------------------------
    // Observation window
    // -------------------------------------------------------------------------
    // Wirekrak is poll-driven.
    // If you do not call poll(), nothing progresses.
    auto start = std::chrono::steady_clock::now();

    while (running.load(std::memory_order_relaxed) && std::chrono::steady_clock::now() - start < runtime) {
        connection.poll();  // Poll-driven execution
        drain_signals();    // Drain any pending signals
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
    std::cout << "\n=== Connection Telemetry ===" << std::endl;
    telemetry.debug_dump(std::cout);

    std::cout << "\n=== WebSocket Telemetry ===" << std::endl;
    telemetry.websocket.debug_dump(std::cout);

    // -------------------------------------------------------------------------
    // Interpretation guide
    // -------------------------------------------------------------------------
    std::cout << "\n=== How to read this ===\n"
        << "- In a stable network, connect success should be 1\n"
        << "- Disconnect events should be exactly 1\n"
        << "- No messages forwarded (no peek_message() calls)\n"
        << "- Telemetry reflects facts, not guesses\n\n"
        << "This is the smallest correct poll-driven Connection program."
        << std::endl;

    return 0;
}
