/*
===============================================================================
 transport::Connection — Group J Unit Tests
 Shutdown & Destructor Guarantees
===============================================================================

Scope:
------
These tests validate deterministic shutdown behavior.

They ensure:
- close() performs a clean, one-time shutdown
- Destructor closes the transport safely
- No retries or callbacks occur after shutdown
- close() is idempotent

These tests MUST NOT involve reconnection scripts beyond initial setup.

===============================================================================
*/

#include <iostream>
#include <memory>

#include "common/harness/connection.hpp"
#include "common/mock_websocket_script.hpp"

// -----------------------------------------------------------------------------
// J1. close() performs graceful shutdown
// -----------------------------------------------------------------------------

void test_close_graceful_shutdown() {
    std::cout << "[TEST] Group J1: close() performs graceful shutdown\n";

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    h.connection->close();

    h.connection->poll();

    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 1);
    TEST_CHECK(h.disconnect_signals == 1);
    TEST_CHECK(h.retry_schedule_signals == 0);

    // Transport must be closed exactly once
    TEST_CHECK(WebSocketUnderTest::close_count() == 1);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// J2. Destructor closes active transport
// -----------------------------------------------------------------------------

void test_destructor_closes_transport() {
    std::cout << "[TEST] Group J2: destructor closes transport\n";

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 1);  // Initial connect

    h.destroy_connection();  // The connection object is no longer observable

    h.drain_signals();    // None to drain

    // Check signals
    TEST_CHECK(h.connect_signals == 1);        // No new connect signals after destruction
    TEST_CHECK(h.disconnect_signals == 0);     // IMPOSSIBLE to observe side effects of an object after its storage has been destroyed.
    TEST_CHECK(h.retry_schedule_signals == 0); // IMPOSSIBLE to observe side effects of an object after its storage has been destroyed.

    // Destructor must have closed transport
    TEST_CHECK(WebSocketUnderTest::close_count() == 1);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// J3. close() is idempotent
// -----------------------------------------------------------------------------

void test_close_idempotent() {
    std::cout << "[TEST] Group J3: close() is idempotent\n";

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    h.connection->close();
    h.connection->close();
    h.connection->close();

    h.connection->poll();
    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 1);
    TEST_CHECK(h.disconnect_signals == 1);
    TEST_CHECK(h.retry_schedule_signals == 0);

    // Transport must be closed exactly once
    TEST_CHECK(WebSocketUnderTest::close_count() == 1);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// J4. Destructor does not schedule reconnect
// -----------------------------------------------------------------------------
//
// Contract:
// ---------
// - Retry scheduling is a semantic transition
// - Transitions are observable ONLY while the Connection object is alive
// - Destructor terminates all semantic emission
//
// This test verifies that:
// - No RetryScheduled signal is emitted before destruction
// - Destructor does not cause retry scheduling
// -----------------------------------------------------------------------------

void test_destructor_no_reconnect() {
    std::cout << "[TEST] Group J4: destructor does not schedule reconnect\n";

    test::MockWebSocketScript script;
    script.connect_ok();

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    // Step initial connect
    script.step(h.connection->ws());

    h.drain_signals();

    // Exactly one connect signal
    TEST_CHECK(h.connect_signals == 1);
    TEST_CHECK(h.retry_schedule_signals == 0);

    // Simulate retriable transport failure
    assert(h.connection->ws() && "harness::Connection::ws() used while transport is null");
    h.connection->ws()->emit_error(Error::RemoteClosed);
    h.connection->ws()->close();

    // IMPORTANT:
    // We intentionally do NOT call poll().
    // Retry scheduling only occurs during poll().
    //
    // Now destroy the connection.
    h.destroy_connection();

    // No further signals are observable after destruction
    h.drain_signals();

    // Assertions
    TEST_CHECK(h.connect_signals == 1);          // initial connect only
    TEST_CHECK(h.retry_schedule_signals == 0);   // no retry scheduled
    TEST_CHECK(h.disconnect_signals == 0);       // destructor is not a semantic transition

    // Transport must have been closed
    TEST_CHECK(WebSocketUnderTest::close_count() == 1);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test entry point
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    test_close_graceful_shutdown();
    test_destructor_closes_transport();
    test_close_idempotent();
    test_destructor_no_reconnect();

    std::cout << "\n[ALL GROUP J TESTS PASSED]\n";
    return 0;
}
