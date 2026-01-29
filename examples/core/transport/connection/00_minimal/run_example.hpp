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

inline int run_example(const char* name, const char* url, const char* description,
                       std::chrono::seconds runtime = std::chrono::seconds(10)) {
    std::signal(SIGINT, on_signal);

    std::cout << "=== Wirekrak Connection - Minimal Lifecycle (" << name << ") ===\n\n"
        << description << "\n\n";

    // -------------------------------------------------------------------------
    // Connection setup
    // -------------------------------------------------------------------------
    // Telemetry is mandatory - it is not optional in Wirekrak.
    wirekrak::core::transport::telemetry::Connection telemetry;

    // Connection owns the logical lifecycle, retries, and liveness.
    wirekrak::core::transport::Connection<WS> connection(telemetry);

    // -------------------------------------------------------------------------
    // Lifecycle callbacks
    // -------------------------------------------------------------------------

    connection.on_connect([&] {
        std::cout << "[example] Connected\n";
    });

    connection.on_disconnect([&] {
        std::cout << "[example] Disconnected\n";
    });

    // Note:
    // No message callback is installed on purpose.
    // This example is about lifecycle, not data.

    // -------------------------------------------------------------------------
    // Open connection
    // -------------------------------------------------------------------------
    std::cout << "[example] Connecting to " << url << "\n";
    if (connection.open(url) != wirekrak::core::transport::Error::None) {
        std::cerr << "[example] Failed to connect\n";
        return 1;
    }

    // -------------------------------------------------------------------------
    // Observation window
    // -------------------------------------------------------------------------
    // Wirekrak is poll-driven.
    // If you do not call poll(), nothing progresses.
    auto start = std::chrono::steady_clock::now();

    while (running.load(std::memory_order_relaxed) &&
           std::chrono::steady_clock::now() - start < runtime) {
        connection.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // -------------------------------------------------------------------------
    // Close connection
    // -------------------------------------------------------------------------
    std::cout << "[example] Closing connection\n";
    connection.close();

    // Drain remaining events (~200 ms)
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
    std::cout << "\n=== How to read this ===\n"
        << "- Connect success should be exactly 1\n"
        << "- Disconnect events should be exactly 1\n"
        << "- No messages forwarded (by design)\n"
        << "- Telemetry reflects facts, not guesses\n\n"
        << "This is the smallest correct Connection program.\n";

    return 0;
}
