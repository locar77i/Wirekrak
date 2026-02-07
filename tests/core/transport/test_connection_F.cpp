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
  - last heartbeat activity

No callbacks, hooks, timers, or background execution are involved.

-------------------------------------------------------------------------------
Liveness semantics
-------------------------------------------------------------------------------
- Liveness transitions are evaluated during poll()
- A timeout occurs ONLY when BOTH heartbeat and message signals are stale
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
F1. Both heartbeat and message stale
    - Liveness transitions to TimedOut
    - Transport is force-closed
    - State machine proceeds to reconnect path

F2. Only heartbeat stale
    - Liveness remains Healthy

F3. Only message stale
    - Liveness remains Healthy

F4. Liveness state transitions
    - Healthy → Warning → TimedOut
    - No repeated transitions across polls
    - Reset to Healthy on reconnection

===============================================================================
*/

#include <cassert>
#include <iostream>
#include <chrono>

#include "common/mock_websocket_script.hpp"
#include "common/connection_harness.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::transport;


// -----------------------------------------------------------------------------
// F1. Both heartbeat and message stale → liveness transitions to TimedOut
// -----------------------------------------------------------------------------
void test_liveness_both_stale() {
    std::cout << "[TEST] Group F1: both heartbeat and message stale\n";

    test::ConnectionHarness h;

    // Establish connection
    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    // Force both timestamps far into the past
    const auto stale = std::chrono::steady_clock::now() - std::chrono::seconds(60);
    h.connection->force_last_message(stale);
    h.connection->force_last_heartbeat(stale);

    // Drive liveness state evaluation
    h.connection->poll();

    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 1);
    TEST_CHECK(h.disconnect_signals == 1);        // Disconnection must occur due to liveness timeout
    TEST_CHECK(h.retry_schedule_signals == 0);
    TEST_CHECK(h.liveness_warning_signals == 1);  // One single liveness warning signal

    // Transport must be force-closed to enter reconnect path
    TEST_CHECK(test::MockWebSocket::close_count() == 1);

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// F2. Only heartbeat stale → no liveness timeout
// -----------------------------------------------------------------------------
void test_liveness_only_heartbeat_stale() {
    std::cout << "[TEST] Group F2: only heartbeat stale\n";

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    const auto now   = std::chrono::steady_clock::now();
    const auto stale = now - std::chrono::seconds(60);

    // Message fresh, heartbeat stale
    h.connection->force_last_message(now);
    h.connection->force_last_heartbeat(stale);

    // Drive liveness state evaluation
    h.connection->poll();

    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 1);
    TEST_CHECK(h.disconnect_signals == 0);
    TEST_CHECK(h.retry_schedule_signals == 0);
    TEST_CHECK(h.liveness_warning_signals == 0);  // No liveness warning

    // Transport must not be closed
    TEST_CHECK(test::MockWebSocket::close_count() == 0);

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// F3. Only message stale → no liveness timeout
// -----------------------------------------------------------------------------
void test_liveness_only_message_stale() {
    std::cout << "[TEST] Group F3: only message stale\n";

    test::ConnectionHarness h;

    using clock = std::chrono::steady_clock;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    const auto now   = std::chrono::steady_clock::now();
    const auto stale = now - std::chrono::seconds(60);

    // Heartbeat fresh, message stale
    h.connection->force_last_message(stale);
    h.connection->force_last_heartbeat(now);

    // Drive liveness state evaluation
    h.connection->poll();

    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 1);
    TEST_CHECK(h.disconnect_signals == 0);
    TEST_CHECK(h.retry_schedule_signals == 0);
    TEST_CHECK(h.liveness_warning_signals == 0); // No liveness warning

    // Transport must not be closed
    TEST_CHECK(test::MockWebSocket::close_count() == 0);

    std::cout << "[TEST] OK\n";
}



// -----------------------------------------------------------------------------
//  Group F4 — Liveness Edge Semantics
// -----------------------------------------------------------------------------
// Validates edge-triggered liveness behavior in transport::Connection.
//
// - Liveness is evaluated during poll()
// - Warning is emitted once per silence window (LivenessThreatened)
// - Timeout requires both heartbeat and message to be stale
// - Timeout forces disconnect (Disconnected)
// - Reconnection resets liveness tracking
//
// Only externally observable signals are asserted.
// -----------------------------------------------------------------------------
void test_connection_liveness_edges()
{
    std::cout << "[TEST] Group F4: liveness edge semantics\n";

    test::ConnectionHarness h(
        std::chrono::seconds(5),   // heartbeat timeout
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
    h.connection->force_last_heartbeat(now - std::chrono::seconds(4));
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
    h.connection->force_last_heartbeat(now - std::chrono::seconds(7));
    h.connection->poll();
    h.drain_signals();

    TEST_CHECK(h.disconnect_signals == 1);
    TEST_CHECK(test::MockWebSocket::close_count() == 1);

    // -------------------------------------------------------------------------
    // Reconnect resets liveness window
    // -------------------------------------------------------------------------
    h.connection->poll(); // drive reconnect
    h.drain_signals();

    TEST_CHECK(h.connect_signals == 2);

    // Silence again → warning must be allowed again
    const auto later = std::chrono::steady_clock::now();
    h.connection->force_last_message(later - std::chrono::seconds(4));
    h.connection->force_last_heartbeat(later - std::chrono::seconds(4));
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

    test_liveness_both_stale();
    test_liveness_only_heartbeat_stale();
    test_liveness_only_message_stale();
    test_connection_liveness_edges();

    std::cout << "\n[GROUP F — LIVENESS DETECTION TESTS PASSED]\n";
    return 0;
}
