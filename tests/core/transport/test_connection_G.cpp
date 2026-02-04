/*
===============================================================================
 transport::Connection — Group G Unit Tests (FINAL)
===============================================================================

Scope:
------
These tests validate the reconnection state machine driven by poll().

Key invariants tested:
----------------------
- Immediate reconnect on retriable transport failure
- Failed reconnect schedules exponential backoff and emits on_retry
- Successful reconnect resets retry state
- Backoff timing is respected *only after a failed reconnect*

Design contract clarified:
--------------------------
The first reconnect attempt after a retriable transport failure is
intentionally immediate. There is no delay or backoff before this attempt.

Exponential backoff applies only after a reconnect attempt fails.
Tests MUST NOT expect poll() to "do nothing" before the first retry.

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

    h.drain_events();

    // Assertions
    TEST_CHECK(h.connect_events == 1);

    // Transport failure + close
    script.step(h.connection->ws());
    script.step(h.connection->ws());

    // Reconnect happens immediately in poll()
    h.connection->poll();

    // Reconnect succeeds
    script.step(h.connection->ws());

    h.drain_events();

    // Check events
    TEST_CHECK(h.connect_events == 2);
    TEST_CHECK(h.disconnect_events == 1);
    TEST_CHECK(h.retry_schedule_events == 0);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// G2. Failed reconnect schedules backoff and invokes on_retry
// -----------------------------------------------------------------------------
void test_failed_reconnect_schedules_backoff() {
    std::cout << "[TEST] Group G2: failed reconnect schedules backoff\n";
    test::MockWebSocket::reset();

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .error(Error::RemoteClosed)
        .close()
        .connect_fail(Error::ConnectionFailed); // first reconnect FAILS

    telemetry::Connection telemetry;
    Connection<test::MockWebSocket> connection{telemetry};

    int retry_calls = 0;
    RetryContext last_retry{};

    connection.on_retry([&](const RetryContext& ctx) {
        ++retry_calls;
        last_retry = ctx;
    });

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(connection.ws());

    // Transport failure + close
    script.step(connection.ws());
    script.step(connection.ws());

    // Arm reconnect failure BEFORE poll
    script.step(connection.ws());

    // poll() triggers reconnect → failure
    connection.poll();

    TEST_CHECK(retry_calls == 1);
    TEST_CHECK(last_retry.attempt == 2);
    TEST_CHECK(last_retry.error == Error::RemoteClosed);

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

    h.drain_events();

    // First connection event
    TEST_CHECK(h.connect_events == 1);

    // Transport failure + close
    script.step(h.connection->ws());
    script.step(h.connection->ws());

    // Reconnect attempt succeeds
    h.connection->poll();
    script.step(h.connection->ws());

    h.drain_events();

    // Check events
    TEST_CHECK(h.connect_events == 2);      // initial + reconnect
    TEST_CHECK(h.disconnect_events == 1);   // single disconnect
    TEST_CHECK(h.retry_schedule_events == 0);        // SUCCESS ⇒ no retry callback

    std::cout << "[TEST] OK\n";
}



/*
===============================================================================
G4. Retry root cause remains stable across multiple failed attempts
===============================================================================

Scope:
------
This test validates retry-cycle stability across multiple failed reconnect
attempts.

Specifically, it ensures that:
- A retry cycle is rooted in the *initial* retriable transport error
- Subsequent reconnect failures with different error types do NOT overwrite
  the retry root cause
- RetryContext.error remains stable for the entire retry cycle

Timing Notes:
-------------
This test intentionally uses real wall-clock time (sleep_for) to allow
reconnection backoff delays to elapse.

The Connection state machine is poll-driven and enforces backoff timing.
Repeated poll() calls MUST NOT bypass backoff.

Do NOT remove or "optimize away" the sleep in this test unless the Connection
is refactored to support injected or mockable clocks.

Design Contract Protected:
--------------------------
- Retry root cause is immutable across a retry cycle
- Backoff applies only after a failed reconnect attempt
- Immediate reconnect attempts are silent and delay-free

===============================================================================
*/

void test_retry_root_cause_stability() {
    std::cout << "[TEST] Group G4: retry root cause stability\n";
    test::MockWebSocket::reset();

    test::MockWebSocketScript script;
    script
        // Initial connection
        .connect_ok()

        // Transport failure triggers retry cycle
        .error(Error::RemoteClosed)
        .close()

        // Immediate reconnect attempt (attempt 1) FAILS
        .connect_fail(Error::ConnectionFailed)

        // First backoff retry attempt (attempt 2) FAILS
        .connect_fail(Error::Timeout);

    telemetry::Connection telemetry;
    Connection<test::MockWebSocket> connection{telemetry};

    int retry_calls = 0;
    std::vector<RetryContext> retries;

    connection.on_retry([&](const RetryContext& ctx) {
        ++retry_calls;
        retries.push_back(ctx);
    });

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(connection.ws());

    // Transport error + close
    script.step(connection.ws());
    script.step(connection.ws());

    // Immediate reconnect attempt fails
    script.step(connection.ws());
    connection.poll();

    // Wait for backoff
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    connection.poll();

    // First backoff retry attempt fails
    script.step(connection.ws());
    connection.poll();

    // Assertions
    TEST_CHECK(retry_calls == 2);

    TEST_CHECK(retries[0].attempt == 2);
    TEST_CHECK(retries[1].attempt == 3);

    TEST_CHECK(retries[0].error == Error::RemoteClosed);
    TEST_CHECK(retries[1].error == Error::RemoteClosed);

    std::cout << "[TEST] OK\n";
}


/*
===============================================================================
 G5. Retry aborts on non-retriable reconnect failure
===============================================================================

Scope:
------
This test validates that an active retry cycle is immediately aborted when a
reconnect attempt fails with a non-retriable error.

Specifically, it ensures that:
- A retriable transport failure (e.g. RemoteClosed) may start a retry cycle
- The *first* reconnect attempt is always executed immediately
- If that reconnect attempt fails with a non-retriable error (LocalShutdown),
  the retry cycle is terminated immediately
- No exponential backoff is scheduled
- The on_retry callback is NOT invoked

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

    // Transport error + close
    script.step(h.connection->ws());
    script.step(h.connection->ws());

    // Immediate reconnect attempt fails
    script.step(h.connection->ws());
    h.connection->poll();

    h.drain_events();

    // Assertions
    TEST_CHECK(h.connect_events == 1);      // only initial connect
    TEST_CHECK(h.disconnect_events == 1);   // Single disconnect event
    TEST_CHECK(h.retry_schedule_events == 0);        // MUST NOT retry

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

        // Transport failure arms retry
        .error(Error::RemoteClosed)
        .close()

        // User explicitly reopens connection
        .connect_ok(); // clean connect to new URL

    test::ConnectionHarness h;

    // Initial open
    TEST_CHECK(h.connection->open("wss://old.example.com/ws") == Error::None);

    // Initial connect
    script.step(h.connection->ws());

    h.drain_events();

    // First connect event
    TEST_CHECK(h.connect_events == 1);

    // Transport error + close (retry armed)
    script.step(h.connection->ws());
    script.step(h.connection->ws());

    // User explicitly opens a new connection
    TEST_CHECK(h.connection->open("wss://new.example.com/ws") == Error::None);

    // New connection succeeds
    script.step(h.connection->ws());

    h.connection->poll();
    h.drain_events();

    // Check events
    TEST_CHECK(h.connect_events == 2);    // initial + explicit open
    TEST_CHECK(h.disconnect_events == 1); // single disconnect
    TEST_CHECK(h.retry_schedule_events == 0);      // retry cycle was cancelled

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

    h.drain_events();

    // First connect event
    TEST_CHECK(h.connect_events == 1);

    // Call poll() repeatedly with no transport activity
    for (int i = 0; i < 100; ++i) {
        h.connection->poll();
    }

    h.drain_events();

    // Check events: absolutely nothing happens
    TEST_CHECK(h.connect_events == 1);
    TEST_CHECK(h.disconnect_events == 0);
    TEST_CHECK(h.retry_schedule_events == 0);

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
    test_open_cancels_retry_cycle();
    test_poll_is_noop_while_connected();

    std::cout << "\n[GROUP G — RECONNECTION LOGIC TESTS PASSED]\n";
    return 0;
}
