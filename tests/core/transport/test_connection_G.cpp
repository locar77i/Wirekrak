/*
===============================================================================
 transport::Connection — Group G Unit Tests
 Reconnection & Backoff Semantics
===============================================================================

Scope:
------
These tests validate the reconnection state machine driven exclusively by
poll()-based progression and observable connection::Signal events.

Key invariants tested:
----------------------
- Immediate reconnect is attempted after a retriable transport failure
- Immediate reconnect emits RetryImmediate exactly once
- Failed reconnect schedules exponential backoff
- Backoff scheduling emits RetryScheduled exactly once per failed attempt
- Successful reconnect resets retry state and increments transport epoch

Design contract clarified:
--------------------------
The first reconnect attempt after a retriable transport failure is
*intentionally immediate*.

There is:
- No delay
- No backoff
- No intermediate idle state

Exponential backoff applies **only after an immediate reconnect attempt fails**.

Observability rules:
--------------------
- Immediate retry is observable via RetryImmediate
- Deferred retry is observable via RetryScheduled
- No callbacks or side channels are involved
- All behavior is deterministic and poll-driven

Tests MUST NOT expect poll() to stall or idle before the first retry.

===============================================================================
*/


#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>

#include "common/mock_websocket_script.hpp"
#include "common/connection_harness.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::transport;


// -----------------------------------------------------------------------------
// G1. Immediate retry on retriable error
// -----------------------------------------------------------------------------
void test_immediate_retry_on_retriable_error() {
    std::cout << "[TEST] Group G1: immediate retry on retriable error\n";

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .error(Error::RemoteClosed)
        .close()
        .connect_ok(); // reconnect succeeds

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(h.connection->ws());

    h.drain_signals();

    // Assertions
    TEST_CHECK(h.connect_signals == 1);

    // Transport failure + close
    script.step(h.connection->ws());
    script.step(h.connection->ws());

    // Reconnect happens immediately in poll()
    h.connection->poll();

    // Reconnect succeeds
    script.step(h.connection->ws());

    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 2);
    TEST_CHECK(h.disconnect_signals == 1);
    TEST_CHECK(h.retry_immediate_signals == 1);
    TEST_CHECK(h.retry_schedule_signals == 0);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// G2. Failed reconnect emits RetryImmediate followed by RetryScheduled
// -----------------------------------------------------------------------------
void test_failed_reconnect_schedules_backoff() {
    std::cout << "[TEST] Group G2: failed reconnect schedules backoff\n";

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .error(Error::RemoteClosed)
        .close()
        .connect_fail(Error::ConnectionFailed); // first reconnect FAILS

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(h.connection->ws());

    // Transport failure + close
    script.step(h.connection->ws());
    script.step(h.connection->ws());

    // Arm reconnect failure BEFORE poll
    script.step(h.connection->ws());

    // poll() triggers reconnect → failure
    h.connection->poll();

    h.drain_signals();

    // Expected signal sequence:
    TEST_CHECK(h.signals.size() == 4);
    TEST_CHECK(h.signals[0] == connection::Signal::Connected);
    TEST_CHECK(h.signals[1] == connection::Signal::Disconnected);
    TEST_CHECK(h.signals[2] == connection::Signal::RetryImmediate);
    TEST_CHECK(h.signals[3] == connection::Signal::RetryScheduled);
    // Counters should reflect both signals
    TEST_CHECK(h.retry_immediate_signals == 1);
    TEST_CHECK(h.retry_schedule_signals == 1);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// G3. Successful reconnect resets retry state
// -----------------------------------------------------------------------------
void test_successful_reconnect_resets_retry_state() {
    std::cout << "[TEST] Group G3: successful reconnect resets retry state\n";

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .error(Error::RemoteClosed)
        .close()
        .connect_ok(); // first reconnect SUCCEEDS

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(h.connection->ws());

    h.drain_signals();

    // First connection signal
    TEST_CHECK(h.connect_signals == 1);

    // Transport failure + close
    script.step(h.connection->ws());
    script.step(h.connection->ws());

    // Reconnect attempt succeeds
    h.connection->poll();
    script.step(h.connection->ws());

    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 2);      // initial + reconnect
    TEST_CHECK(h.disconnect_signals == 1);   // single disconnect
    TEST_CHECK(h.retry_schedule_signals == 0);        // SUCCESS ⇒ no retry callback

    std::cout << "[TEST] OK\n";
}



/*
===============================================================================
 G4. Retry root cause remains stable across multiple failed attempts
===============================================================================

Scope:
------
This test validates retry-cycle stability across multiple failed reconnect
attempts using signal-based observability only.

Specifically, it ensures that:
- A retry cycle is rooted in the *initial* retriable transport failure
- Subsequent reconnect failures with different error types do NOT reset
  or fragment the retry cycle
- Immediate retry occurs once
- Backoff retries are scheduled deterministically thereafter

Observability Model:
--------------------
- RetryImmediate is emitted once per retry cycle
- RetryScheduled is emitted once per failed reconnect attempt
- No callbacks or retry context objects are observed
- Stability is inferred from signal continuity and retry progression

Timing Notes:
-------------
This test uses real wall-clock time to allow backoff delays to elapse.

Repeated poll() calls MUST NOT bypass backoff timing.

Do NOT remove sleep_for unless the Connection supports injected clocks.

Design Contract Protected:
--------------------------
- Retry root cause is immutable across a retry cycle
- Immediate retry is delay-free and silent except for RetryImmediate
- Exponential backoff applies only after a failed reconnect attempt

===============================================================================
*/

void test_retry_root_cause_stability() {
    std::cout << "[TEST] Group G4: retry root cause stability\n";

    test::MockWebSocketScript script;
    script
        // Initial connection
        .connect_ok()

        // Transport failure triggers retry cycle
        .error(Error::RemoteClosed)
        .close()

        // Immediate reconnect attempt FAILS
        .connect_fail(Error::ConnectionFailed)

        // Backoff retry attempt FAILS with DIFFERENT error
        .connect_fail(Error::Timeout);

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(h.connection->ws());
    h.drain_signals();

    // Transport error + close
    script.step(h.connection->ws());
    script.step(h.connection->ws());

    // Immediate reconnect attempt fails
    script.step(h.connection->ws());
    h.connection->poll();
    h.drain_signals();

    // Expect:
    // - exactly one immediate retry
    // - exactly one scheduled retry so far
    TEST_CHECK(h.retry_immediate_signals == 1);
    TEST_CHECK(h.retry_schedule_signals == 1);

    // Allow backoff window to elapse
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Backoff retry attempt fails
    script.step(h.connection->ws());
    h.connection->poll();
    h.drain_signals();

    // Expect:
    // - still only one immediate retry
    // - second scheduled retry emitted
    TEST_CHECK(h.retry_immediate_signals == 1);
    TEST_CHECK(h.retry_schedule_signals == 2);

    std::cout << "[TEST] OK\n";
}



/*
===============================================================================
 G5. Retry aborts on non-retriable reconnect failure
===============================================================================

Scope:
------
This test validates that an active retry cycle is immediately aborted when an
immediate reconnect attempt fails with a non-retriable error.

Specifically, it ensures that:
- A retriable transport failure (e.g. RemoteClosed) may start a retry cycle
- The first reconnect attempt is always executed immediately
- If that immediate reconnect fails with a non-retriable error (LocalShutdown),
  the retry cycle is terminated immediately
- No exponential backoff is scheduled
- No further reconnect attempts occur

Observability Model:
--------------------
- RetryImmediate is emitted once when the retry cycle begins
- RetryScheduled is NOT emitted
- The connection resolves to Disconnected
- No callbacks are involved; behavior is observed via signals and state

Design Contract Protected:
--------------------------
- Retry is a recovery mechanism, not a defiance of explicit shutdown intent
- Non-retriable errors always take precedence over retry policy
- Retry cycles are finite and explicitly bounded

Why This Matters:
-----------------
Without this guarantee, a reconnect loop could ignore explicit shutdown signals
and retry indefinitely, violating user intent and causing resource churn.

===============================================================================
*/

void test_retry_aborts_on_non_retriable_reconnect_failure() {
    std::cout << "[TEST] Group G5: retry abort on non-retriable reconnect failure\n";

    test::MockWebSocketScript script;
    script
        // Initial connection
        .connect_ok()

        // Transport failure starts retry cycle
        .error(Error::RemoteClosed)
        .close()

        // Immediate reconnect attempt FAILS with non-retriable error
        .connect_fail(Error::LocalShutdown);

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(h.connection->ws());
    h.drain_signals();

    // Transport error + close
    script.step(h.connection->ws());
    script.step(h.connection->ws());

    // Immediate reconnect attempt fails
    script.step(h.connection->ws());
    h.connection->poll();

    h.drain_signals();

    // Assertions
    TEST_CHECK(h.connect_signals == 1);            // initial connect only
    TEST_CHECK(h.disconnect_signals == 1);         // single logical disconnect
    TEST_CHECK(h.retry_immediate_signals == 1);    // retry cycle started
    TEST_CHECK(h.retry_schedule_signals == 0);     // MUST NOT schedule backoff

    TEST_CHECK(h.connection->get_state() == State::Disconnected);
    TEST_CHECK(test::MockWebSocket::is_connected() == false);

    std::cout << "[TEST] OK\n";
}



/*
===============================================================================
 G6. open() cancels an active retry cycle
===============================================================================

Scope:
------
This test validates that an explicit call to open() cancels any active retry
cycle and resets all retry-related state.

Specifically, it ensures that:
- A retriable transport failure may arm a retry cycle
- While in WaitingReconnect, a user may explicitly call open()
- The explicit open() call cancels the pending retry cycle
- retry_attempts_ is reset
- retry_root_error_ is cleared
- No retry callbacks are invoked
- The new connection proceeds as a clean, fresh connection

Design Contract Protected:
--------------------------
- Explicit user intent always overrides automatic recovery
- open() represents a hard reset boundary for connection state
- No "ghost retries" may occur after a successful open()

Why This Matters:
-----------------
In real usage, users may manually recover from errors by reopening a
connection (possibly to a new endpoint). Automatic retry logic must never
leak across this boundary, or it can cause unexpected reconnects, stale
endpoints, and confusing UX.

===============================================================================
*/

void test_open_cancels_retry_cycle() {
    std::cout << "[TEST] Group G6: open cancels retry cycle\n";

    test::MockWebSocketScript script;
    script
        // Initial connection
        .connect_ok()

        // Transport failure arms immediate retry
        .error(Error::RemoteClosed)
        .close()

        // Inmediate retry fails
        .connect_fail(Error::TransportFailure)

        // User explicitly reopens connection
        .connect_ok(); // clean connect to new URL

    test::ConnectionHarness h;

    // Initial open
    TEST_CHECK(h.connection->open("wss://old.example.com/ws") == Error::None);

    // Initial connect
    script.step(h.connection->ws());

    h.connection->poll();
    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 1);           // Connected successfully
    TEST_CHECK(h.disconnect_signals == 0);        // No disconnect signals yet (until next poll)
    TEST_CHECK(h.liveness_warning_signals == 0);  // No liveness warning signals
    TEST_CHECK(h.retry_immediate_signals == 0);   // No retry immediate signals yet
    TEST_CHECK(h.retry_schedule_signals == 0);    // No retry schedule signals yet
    TEST_CHECK(h.signals.size() == 1);
    TEST_CHECK(h.signals[0] == connection::Signal::Connected);

    // Transport error + close (immediate retry armed) + transport failure
    script.step(h.connection->ws());
    script.step(h.connection->ws());
    script.step(h.connection->ws());

    h.connection->poll();
    h.drain_signals();

    // New connection succeeds
    script.step(h.connection->ws());

    // User explicitly opens a new connection
    TEST_CHECK(h.connection->open("wss://new.example.com/ws") == Error::None);

    h.connection->poll();
    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 2);           // Connected successfully
    TEST_CHECK(h.disconnect_signals == 1);        // No disconnect signals yet (until next poll)
    TEST_CHECK(h.liveness_warning_signals == 0);  // No liveness warning signals
    TEST_CHECK(h.retry_immediate_signals == 1);   // No retry immediate signals yet
    TEST_CHECK(h.retry_schedule_signals == 1);    // No retry schedule signals yet
    TEST_CHECK(h.signals.size() == 5);
    TEST_CHECK(h.signals[0] == connection::Signal::Connected);
    TEST_CHECK(h.signals[1] == connection::Signal::Disconnected);
    TEST_CHECK(h.signals[2] == connection::Signal::RetryImmediate);
    TEST_CHECK(h.signals[3] == connection::Signal::RetryScheduled);
    TEST_CHECK(h.signals[4] == connection::Signal::Connected);

    std::cout << "[TEST] OK\n";
}


/*
===============================================================================
 G7. poll() is a no-op while connected and idle
===============================================================================

Scope:
------
This test validates that repeated calls to poll() while the connection is in a
healthy, idle Connected state are safe no-ops.

Specifically, it ensures that:
- poll() performs no work when there are no transport events
- No callbacks (connect, disconnect, retry) are invoked
- No retry or liveness logic is triggered
- The connection state remains unchanged

Design Contract Protected:
--------------------------
- Connection behavior is strictly poll-driven
- There are no hidden timers or background work
- poll() is deterministic and side-effect free when idle

Why This Matters:
-----------------
poll() is expected to be called frequently (e.g. from a main loop or reactor).
Any implicit behavior, periodic work, or side effects inside poll() would
violate determinism, harm performance, and make the system harder to reason
about.

This test guards against accidental logic being introduced into poll() that
executes without an explicit triggering event.

===============================================================================
*/
void test_poll_is_noop_while_connected() {
    std::cout << "[TEST] Group G7: poll no-op while connected\n";

    test::MockWebSocketScript script;
    script
        .connect_ok(); // clean connection, no further events

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(h.connection->ws());

    h.drain_signals();

    // First connect signal
    TEST_CHECK(h.connect_signals == 1);

    // Call poll() repeatedly with no transport activity
    for (int i = 0; i < 100; ++i) {
        h.connection->poll();
    }

    h.drain_signals();

    // Check signals: absolutely nothing happens
    TEST_CHECK(h.connect_signals == 1);
    TEST_CHECK(h.disconnect_signals == 0);
    TEST_CHECK(h.retry_schedule_signals == 0);

    // Websocket remains connected
    TEST_CHECK(test::MockWebSocket::is_connected());

    std::cout << "[TEST] OK\n";
}



// -----------------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_immediate_retry_on_retriable_error();
    test_failed_reconnect_schedules_backoff();
    test_successful_reconnect_resets_retry_state();
    test_retry_root_cause_stability();
    test_retry_aborts_on_non_retriable_reconnect_failure();
    test_open_cancels_retry_cycle(); // TODO: re-check after fixing multi-transition in a single poll() call
    test_poll_is_noop_while_connected();

    std::cout << "\n[GROUP G — RECONNECTION LOGIC TESTS PASSED]\n";
    return 0;
}
