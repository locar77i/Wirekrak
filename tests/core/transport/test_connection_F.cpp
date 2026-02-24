/*
===============================================================================
 transport::Connection — Group F Unit Tests (Liveness State Machine)
===============================================================================

Scope:
------
These tests validate the transport-level liveness *state machine* implemented
by transport::Connection.

Liveness is modeled as an explicit, deterministic state:

    Healthy → Warning → TimedOut

and is derived exclusively from observable timestamps:
  - last message activity

No callbacks, hooks, timers, or background execution are involved.

-------------------------------------------------------------------------------
Liveness semantics
-------------------------------------------------------------------------------
- Liveness transitions are evaluated during poll()
- A timeout occurs ONLY when the message signal is stale
- Warning is entered once the danger window is crossed
- Each transition is monotonic and fires at most once per silence window
- Liveness resets to Healthy only on observable traffic

-------------------------------------------------------------------------------
What these tests intentionally avoid
-------------------------------------------------------------------------------
- Transport scripting or message sequencing
- Network I/O simulation
- Backoff timing
- Retry policy validation
- Protocol/session behavior

These tests exercise *pure decision logic* only.

-------------------------------------------------------------------------------
Covered requirements
-------------------------------------------------------------------------------
F1. The message signal is stale
    - Liveness transitions to TimedOut
    - Transport is force-closed
    - State machine proceeds to reconnect path

F2. Liveness state transitions
    - Healthy → Warning → TimedOut
    - No repeated transitions across polls
    - Reset to Healthy on reconnection

===============================================================================
*/

#include <cassert>
#include <iostream>
#include <chrono>

#include "common/harness/connection.hpp"


// -----------------------------------------------------------------------------
// F1. The message signal is stale → liveness transitions to TimedOut
// -----------------------------------------------------------------------------
void test_liveness_stale() {
    std::cout << "[TEST] Group F1: message stale\n";

    test::ConnectionHarness h;

    // Establish connection
    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    // Force both timestamps far into the past
    const auto stale = std::chrono::steady_clock::now() - std::chrono::seconds(60);
    h.connection->force_last_message(stale);

    // Drive liveness state evaluation
    h.connection->poll();
    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 1);           // Connected successfully
    TEST_CHECK(h.disconnect_signals == 0);        // No disconnect signals yet (until next poll)
    TEST_CHECK(h.liveness_warning_signals == 1);  // One single liveness warning signal
    TEST_CHECK(h.retry_immediate_signals == 0);   // No retry immediate signals yet
    TEST_CHECK(h.retry_schedule_signals == 0);    // No retry schedule signals yet
    TEST_CHECK(h.signals.size() == 2);
    TEST_CHECK(h.signals[0] == connection::Signal::Connected);
    TEST_CHECK(h.signals[1] == connection::Signal::LivenessThreatened);

    // Drive inmediate retry logic (due to liveness timeout)
    h.connection->poll();
    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 2);           // Re-connected successfully
    TEST_CHECK(h.disconnect_signals == 1);        // The first disconnect signal (due to liveness timeout)
    TEST_CHECK(h.liveness_warning_signals == 1);  // One single liveness warning signal
    TEST_CHECK(h.retry_immediate_signals == 1);   // One retry immediate signal
    TEST_CHECK(h.retry_schedule_signals == 0);    // No retry schedule signals yet
    TEST_CHECK(h.signals.size() == 5);
    TEST_CHECK(h.signals[0] == connection::Signal::Connected);
    TEST_CHECK(h.signals[1] == connection::Signal::LivenessThreatened);
    TEST_CHECK(h.signals[2] == connection::Signal::Disconnected);
    TEST_CHECK(h.signals[3] == connection::Signal::RetryImmediate);
    TEST_CHECK(h.signals[4] == connection::Signal::Connected);

    // Transport must be force-closed to enter reconnect path
    TEST_CHECK(WebSocketUnderTest::close_count() == 1);

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
//  Group F2 — Liveness Edge Semantics
// -----------------------------------------------------------------------------
// Validates edge-triggered liveness behavior in transport::Connection.
//
// - Liveness is evaluated during poll()
// - Warning is emitted once per silence window (LivenessThreatened)
// - Timeout requires message to be stale
// - Timeout forces disconnect (Disconnected)
// - Reconnection resets liveness tracking
//
// Only externally observable signals are asserted.
// -----------------------------------------------------------------------------
void test_connection_liveness_edges()
{
    std::cout << "[TEST] Group F2: liveness edge semantics\n";

    test::ConnectionHarness h(
        std::chrono::seconds(5),   // message timeout
        0.7                        // warning ratio (70%)
    );

    // -------------------------------------------------------------------------
    // Connect
    // -------------------------------------------------------------------------
    TEST_CHECK(h.connection->open("wss://test") == Error::None);
    h.drain_signals();

    TEST_CHECK(h.connect_signals == 1);
    TEST_CHECK(h.liveness_warning_signals == 0);
    TEST_CHECK(h.disconnect_signals == 0);

    const auto now = std::chrono::steady_clock::now();

    // -------------------------------------------------------------------------
    // Enter warning window
    // -------------------------------------------------------------------------
    h.connection->force_last_message(now - std::chrono::seconds(4));
    h.connection->poll();
    h.drain_signals();

    TEST_CHECK(h.disconnect_signals == 0);
    TEST_CHECK(h.liveness_warning_signals == 1);

    // Poll again — must NOT refire warning
    h.connection->poll();
    h.drain_signals();

    TEST_CHECK(h.liveness_warning_signals == 1); // still exactly one

    // -------------------------------------------------------------------------
    // Enter timeout
    // -------------------------------------------------------------------------
    h.connection->force_last_message(now - std::chrono::seconds(7));
    h.connection->poll();
    h.connection->poll();
    h.drain_signals();

    TEST_CHECK(h.disconnect_signals == 1);
    TEST_CHECK(WebSocketUnderTest::close_count() == 1);

    // -------------------------------------------------------------------------
    // Reconnect resets liveness window
    // -------------------------------------------------------------------------
    h.connection->poll(); // drive reconnect
    h.drain_signals();

    TEST_CHECK(h.connect_signals == 2);

    // Silence again → warning must be allowed again
    const auto later = std::chrono::steady_clock::now();
    h.connection->force_last_message(later - std::chrono::seconds(4));
    h.connection->poll();
    h.drain_signals();

    TEST_CHECK(h.liveness_warning_signals == 2); // new cycle, new warning

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_liveness_stale();
    test_connection_liveness_edges();

    std::cout << "\n[GROUP F — LIVENESS DETECTION TESTS PASSED]\n";
    return 0;
}
