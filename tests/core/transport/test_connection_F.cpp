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

#include "wirekrak/core/transport/connection.hpp"
#include "common/mock_websocket.hpp"
#include "common/mock_websocket_script.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::transport;


// -----------------------------------------------------------------------------
// F1. Both heartbeat and message stale → liveness transitions to TimedOut
// -----------------------------------------------------------------------------
void test_liveness_both_stale() {
    std::cout << "[TEST] Group F1: both heartbeat and message stale\n";
    test::MockWebSocket::reset();

    using clock = std::chrono::steady_clock;

    telemetry::Connection telemetry;
    Connection<test::MockWebSocket> connection{telemetry};

    // Establish connection
    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    // Force both timestamps far into the past
    const auto stale = clock::now() - std::chrono::seconds(60);
    connection.force_last_message(stale);
    connection.force_last_heartbeat(stale);

    // Drive liveness state evaluation
    connection.poll();

    // Liveness state must be TimedOut
    TEST_CHECK(connection.liveness() == Liveness::TimedOut);

    // Transport must be force-closed to enter reconnect path
    TEST_CHECK(test::MockWebSocket::close_count() == 1);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// F2. Only heartbeat stale → no liveness timeout
// -----------------------------------------------------------------------------
void test_liveness_only_heartbeat_stale() {
    std::cout << "[TEST] Group F2: only heartbeat stale\n";
    test::MockWebSocket::reset();

    using clock = std::chrono::steady_clock;

    telemetry::Connection telemetry;
    Connection<test::MockWebSocket> connection{telemetry};

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    const auto now   = clock::now();
    const auto stale = now - std::chrono::seconds(60);

    // Message fresh, heartbeat stale
    connection.force_last_message(now);
    connection.force_last_heartbeat(stale);

    // Drive liveness state evaluation
    connection.poll();

    // No liveness timeout
    TEST_CHECK(connection.liveness() == Liveness::Healthy);

    // Transport must not be closed
    TEST_CHECK(test::MockWebSocket::close_count() == 0);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// F3. Only message stale → no liveness timeout
// -----------------------------------------------------------------------------
void test_liveness_only_message_stale() {
    std::cout << "[TEST] Group F3: only message stale\n";
    test::MockWebSocket::reset();

    using clock = std::chrono::steady_clock;

    telemetry::Connection telemetry;
    Connection<test::MockWebSocket> connection{telemetry};

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    const auto now   = clock::now();
    const auto stale = now - std::chrono::seconds(60);

    // Heartbeat fresh, message stale
    connection.force_last_message(stale);
    connection.force_last_heartbeat(now);

    // Drive liveness state evaluation
    connection.poll();

    // No liveness timeout
    TEST_CHECK(connection.liveness() == Liveness::Healthy);

    // Transport must not be closed
    TEST_CHECK(test::MockWebSocket::close_count() == 0);

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// F4. Liveness state transitions
// -----------------------------------------------------------------------------
//
//   - Liveness is a deterministic state machine
//   - Transitions are monotonic:
//       Healthy -> Warning -> TimedOut
//   - Each transition fires at most once per silence window
//   - Liveness resets to Healthy only on observable traffic
//   - No callbacks, no hooks, no side effects
//
// This test validates transport-level liveness semantics only.
// ============================================================================
void test_connection_liveness_state_transitions()
{
    std::cout << "[TEST] Group F4: liveness state transitions\n";
    test::MockWebSocket::reset();

    telemetry::Connection telemetry;
    Connection<test::MockWebSocket> conn(
        telemetry,
        std::chrono::seconds(5),   // heartbeat timeout
        std::chrono::seconds(5),   // message timeout
        0.8                        // warning ratio (80%)
    );   

    // -------------------------------------------------------------------------
    // Connect
    // -------------------------------------------------------------------------
    TEST_CHECK(conn.open("wss://test") == Error::None);
    TEST_CHECK(conn.get_state() == State::Connected);
    TEST_CHECK(conn.liveness() == Liveness::Healthy);

    const auto now = std::chrono::steady_clock::now();

    // -------------------------------------------------------------------------
    // Still healthy (inside safe window)
    // -------------------------------------------------------------------------
    conn.force_last_message(now - std::chrono::seconds(2));
    conn.force_last_heartbeat(now - std::chrono::seconds(2));
    conn.poll();

    TEST_CHECK(conn.get_state() == State::Connected);
    TEST_CHECK(conn.liveness() == Liveness::Healthy);

    // -------------------------------------------------------------------------
    // Enter warning window
    // -------------------------------------------------------------------------
    conn.force_last_message(now - std::chrono::seconds(4));
    conn.force_last_heartbeat(now - std::chrono::seconds(4));
    conn.poll();

    TEST_CHECK(conn.get_state() == State::Connected);
    TEST_CHECK(conn.liveness() == Liveness::Warning);

    // Poll again — must NOT regress or refire
    conn.poll();

    TEST_CHECK(conn.get_state() == State::Connected);
    TEST_CHECK(conn.liveness() == Liveness::Warning);

    // -------------------------------------------------------------------------
    // Enter timeout
    // -------------------------------------------------------------------------
    conn.force_last_message(now - std::chrono::seconds(7));
    conn.force_last_heartbeat(now - std::chrono::seconds(7));
    conn.poll();

    TEST_CHECK(conn.get_state() == State::WaitingReconnect);
    TEST_CHECK(conn.liveness() == Liveness::TimedOut);

    // Poll again — must reconnect
    conn.poll();
    TEST_CHECK(conn.get_state() == State::Connected);
    TEST_CHECK(conn.liveness() == Liveness::Healthy);

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
    test_connection_liveness_state_transitions();

    std::cout << "\n[GROUP F — LIVENESS DETECTION TESTS PASSED]\n";
    return 0;
}
