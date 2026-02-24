/*
===============================================================================
 transport::Connection — Group B Unit Tests
 open() semantics & caller intent
===============================================================================

Scope:
------
These tests validate the externally observable semantics of Connection::open().

They focus on:
- Explicit caller intent
- Deterministic state-machine transitions
- Observable lifecycle signals
- Correct rejection of invalid usage

IMPORTANT:
----------
These tests validate *observable consequences*, not internal state.
They assert behavior exclusively through:
- return values
- connection::Signal edges
- transport mock effects

Transport behavior is fully mocked and deterministic.
No timing assumptions, sleeps, or background threads are involved.

-------------------------------------------------------------------------------
Covered Contracts
-------------------------------------------------------------------------------

B1. open() succeeds from Disconnected
    - Transport connect succeeds
    - Logical lifecycle enters Connected
    - connection::Signal::Connected emitted exactly once

B2. open() fails with retriable error
    - Transport returns a retriable failure
    - Logical connection does NOT enter Connected
    - Retry cycle becomes observable on poll()
    - connection::Signal::RetryScheduled emitted

B3. open() fails with non-retriable error
    - Failure resolved synchronously
    - Logical connection returns to Disconnected
    - No retry scheduled
    - No lifecycle signals emitted

B4. open() called while already connected
    - Rejected as invalid caller intent
    - No state change
    - No lifecycle signals emitted

-------------------------------------------------------------------------------
Non-Goals
-------------------------------------------------------------------------------
- Backoff timing
- Retry attempt counts
- Transport close semantics
- Liveness detection

===============================================================================
*/


#include <cassert>
#include <iostream>
#include <string>

#include "common/harness/connection.hpp"


// -----------------------------------------------------------------------------
// B1. open() establishes a logical connection
// -----------------------------------------------------------------------------
void test_open_success() {
    std::cout << "[TEST] Group B1: open() succeeds from Disconnected\n";

    test::ConnectionHarness h;

    // Default MockWebSocket connect result is Error::None
    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    h.drain_signals();

    // Connected edge must be emitted exactly once
    TEST_CHECK(h.connect_signals == 1);

    // Transport must be connected
    TEST_CHECK(h.connection->ws().is_connected());

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// B2. open() fails with retriable transport error
// -----------------------------------------------------------------------------
void test_open_retriable_failure() {
    std::cout << "[TEST] Group B2: open() fails with retriable error\n";

    test::ConnectionHarness h;

    // Force next connect attempt to fail with a retriable error
    WebSocketUnderTest::set_next_connect_result(Error::ConnectionFailed);

    // open() must return the transport error
    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::ConnectionFailed);

    h.drain_signals();

    // Connected edge must not be emitted
    TEST_CHECK(h.connect_signals == 0);

    // Transport must not be connected
    TEST_CHECK(h.connection->ws().is_connected() == false);

    // Reconnect is scheduled implicitly:
    // observable behavior → calling poll() must attempt reconnect
    h.connection->poll();

    h.drain_signals();

    // Check connection signals
    TEST_CHECK(h.connect_signals == 0);     // No connect calls
    TEST_CHECK(h.disconnect_signals == 0);  // No disconnect calls
    TEST_CHECK(h.retry_schedule_signals == 1);       // (retry_attempts_ == 1 internally)

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// B3. Failure is resolved synchronously; poll() must not change outcome
// -----------------------------------------------------------------------------
void test_open_non_retriable_failure() {
    std::cout << "[TEST] Group B3: open() fails with non-retriable error\n";

    test::ConnectionHarness h;

    // Invalid URL → parse_and_connect_ fails before transport retry logic
    TEST_CHECK(h.connection->open("invalid://url") == Error::InvalidUrl);

    h.drain_signals();

    // No connect calls
    TEST_CHECK(h.connect_signals == 0);

    // poll() must not trigger reconnect attempts
    h.connection->poll();

    h.drain_signals();

    // Check connection signals
    TEST_CHECK(h.connect_signals == 0);     // No connect calls
    TEST_CHECK(h.disconnect_signals == 0);  // No disconnect calls
    TEST_CHECK(h.retry_schedule_signals == 0);       // No retries scheduled

    // Transport should never have been connected
    TEST_CHECK(WebSocketUnderTest::close_count() == 0);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// B4. Second open() is rejected as invalid caller intent
// -----------------------------------------------------------------------------
void test_open_while_connected() {
    std::cout << "[TEST] Group B4: open() while already connected\n";

    test::ConnectionHarness h;

    // First open succeeds
    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    h.drain_signals();

    TEST_CHECK(h.connect_signals == 1);

    // Second open must fail with InvalidState
    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::InvalidState);

    h.drain_signals();

    // Check connection signals
    TEST_CHECK(h.connect_signals == 1);
    TEST_CHECK(h.disconnect_signals == 0);
    TEST_CHECK(h.retry_schedule_signals == 0);

    // Transport remains connected
    TEST_CHECK(h.connection->ws().is_connected() == true);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_open_success();
    test_open_retriable_failure();
    test_open_non_retriable_failure();
    test_open_while_connected();

    std::cout << "\n[GROUP B — OPEN() SEMANTICS TESTS PASSED]\n";
    return 0;
}
