/*
===============================================================================
 transport::Connection — Group B Unit Tests
===============================================================================

Scope:
------
These tests validate the semantics of Connection::open(), focusing on
explicit caller intent and correct state-machine transitions.

This group verifies:
- Successful connection establishment
- Failure handling (retriable vs non-retriable)
- Callback guarantees
- Correct behavior when open() is misused

Transport behavior is fully mocked and deterministic.
No timing assumptions, sleeps, or background threads are involved.

Covered Requirements:
---------------------
B1. open() succeeds from Disconnected
    - Transport connect succeeds
    - Disconnected → Connecting → Connected
    - on_connect fires exactly once

B2. open() fails with retriable error
    - Transport returns ConnectionFailed
    - Disconnected → Connecting → WaitingReconnect
    - No on_connect
    - retry_attempts_ initialized to 1 (observable via behavior)

B3. open() fails with non-retriable error
    - Transport returns InvalidUrl
    - Disconnected → Connecting → Disconnected
    - No retry scheduled
    - No callbacks

B4. open() called while already connected
    - Returns Error::InvalidState
    - No state change
    - No callbacks fired

Non-Goals:
----------
- Reconnection timing
- Backoff policy
- Transport close semantics
- Liveness detection

===============================================================================
*/

#include <cassert>
#include <iostream>
#include <string>

#include "common/connection_harness.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::transport;


// -----------------------------------------------------------------------------
// B1. open() succeeds from Disconnected
// -----------------------------------------------------------------------------
void test_open_success() {
    std::cout << "[TEST] Group B1: open() succeeds from Disconnected\n";

    test::ConnectionHarness h;

    // Default MockWebSocket connect result is Error::None
    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    h.drain_events();

    // on_connect must fire exactly once
    TEST_CHECK(h.connect_events == 1);

    // Transport must be connected
    TEST_CHECK(h.connection->ws().is_connected());

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// B2. open() fails with retriable error
// -----------------------------------------------------------------------------
void test_open_retriable_failure() {
    std::cout << "[TEST] Group B2: open() fails with retriable error\n";

    test::ConnectionHarness h;

    // Force next connect attempt to fail with a retriable error
    test::MockWebSocket::set_next_connect_result(Error::ConnectionFailed);

    // open() must return the transport error
    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::ConnectionFailed);

    h.drain_events();

    // on_connect must NOT be called
    TEST_CHECK(h.connect_events == 0);

    // Transport must not be connected
    TEST_CHECK(h.connection->ws().is_connected() == false);

    // Reconnect is scheduled implicitly:
    // observable behavior → calling poll() must attempt reconnect
    h.connection->poll();

    h.drain_events();

    // Check connection events
    TEST_CHECK(h.connect_events == 0);     // No connect calls
    TEST_CHECK(h.disconnect_events == 0);  // No disconnect calls
    TEST_CHECK(h.retry_schedule_events == 1);       // (retry_attempts_ == 1 internally)

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// B3. open() fails with non-retriable error
// -----------------------------------------------------------------------------
void test_open_non_retriable_failure() {
    std::cout << "[TEST] Group B3: open() fails with non-retriable error\n";

    test::ConnectionHarness h;

    // Invalid URL → parse_and_connect_ fails before transport retry logic
    TEST_CHECK(h.connection->open("invalid://url") == Error::InvalidUrl);

    h.drain_events();

    // No connect calls
    TEST_CHECK(h.connect_events == 0);

    // poll() must not trigger reconnect attempts
    h.connection->poll();

    h.drain_events();

    // Check connection events
    TEST_CHECK(h.connect_events == 0);     // No connect calls
    TEST_CHECK(h.disconnect_events == 0);  // No disconnect calls
    TEST_CHECK(h.retry_schedule_events == 0);       // No retries scheduled

    // Transport should never have been connected
    TEST_CHECK(test::MockWebSocket::close_count() == 0);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// B4. open() called while already connected
// -----------------------------------------------------------------------------
void test_open_while_connected() {
    std::cout << "[TEST] Group B4: open() while already connected\n";

    test::ConnectionHarness h;

    // First open succeeds
    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    h.drain_events();

    TEST_CHECK(h.connect_events == 1);

    // Second open must fail with InvalidState
    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::InvalidState);

    h.drain_events();

    // Check connection events
    TEST_CHECK(h.connect_events == 1);
    TEST_CHECK(h.disconnect_events == 0);
    TEST_CHECK(h.retry_schedule_events == 0);

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
