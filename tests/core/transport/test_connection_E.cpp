/*
===============================================================================
 transport::Connection — Group E Unit Tests
 Transport closure observability & retry consequences
===============================================================================

Scope:
------
These tests validate the *observable consequences* of unexpected transport
closure as exposed by connection::Signal.

They ensure:
- Disconnected signals are emitted exactly once
- Reconnect attempts produce new Connected signals when allowed
- Retry scheduling signals are emitted only when applicable
- Non-retriable closures do not trigger retry behavior

IMPORTANT TESTING RULE:
-----------------------
Reconnect attempts occur synchronously inside poll().

All transport outcomes MUST be scripted via MockWebSocketScript
*before* calling poll().

===============================================================================
*/

#include <cassert>
#include <iostream>

#include "common/harness/connection.hpp"
#include "common/mock_websocket_script.hpp"


// -----------------------------------------------------------------------------
// E1. Transport close while Connected (retriable)
// -----------------------------------------------------------------------------
void test_transport_close_retriable() {
    std::cout << "[TEST] Group E1: transport close retriable\n";

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .error(Error::RemoteClosed)
        .close()
        .connect_ok();              // reconnect succeeds

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(h.connection->ws());

    h.drain_signals();

    // Assertions
    TEST_CHECK(h.connect_signals == 1);

    // Error from transport
    script.step(h.connection->ws());

    // Transport closes
    script.step(h.connection->ws());

    // poll triggers reconnect
    h.connection->poll();

    // Reconnect succeeds
    script.step(h.connection->ws());

    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 2);         // initial + reconnect
    TEST_CHECK(h.disconnect_signals == 1);      // Single disconnect
    TEST_CHECK(h.retry_schedule_signals == 0);  // No retry scheduled

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// E2. Transport close while Connected (non-retriable)
// -----------------------------------------------------------------------------
void test_transport_close_non_retriable() {
    std::cout << "[TEST] Group E2: transport close non-retriable\n";

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .error(Error::LocalShutdown)
        .close();

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    // Step connect
    script.step(h.connection->ws());

    // Step explicit non-retriable error
    script.step(h.connection->ws());

    // Step close
    script.step(h.connection->ws());

    // Drive state machine
    h.connection->poll();

    // Drain signals and check
    h.drain_signals();

    // Disconnect callback fires once
    TEST_CHECK(h.disconnect_signals == 1);

    // No retry scheduled
    TEST_CHECK(h.retry_schedule_signals == 0);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// E3. Transport close while Disconnecting
// -----------------------------------------------------------------------------
void test_transport_close_while_disconnecting() {
    std::cout << "[TEST] Group E3: transport close while Disconnecting\n";

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .close();

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    // Step connect
    script.step(h.connection->ws());

    // User initiates shutdown
    h.connection->close();

    // Transport reports close
    script.step(h.connection->ws());

    // Drive state machine
    h.connection->poll();

    h.drain_signals();

    // Disconnect fires once
    TEST_CHECK(h.disconnect_signals == 1);

    // No retry scheduled
    TEST_CHECK(h.retry_schedule_signals == 0);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_transport_close_retriable();
    test_transport_close_non_retriable();
    test_transport_close_while_disconnecting();

    std::cout << "\n[GROUP E — TRANSPORT CLOSURE TESTS PASSED]\n";
    return 0;
}
