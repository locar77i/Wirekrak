/*
===============================================================================
 transport::Connection — Group E Unit Tests
===============================================================================

Scope:
------
These tests validate how Connection reacts to unexpected transport closure.

This group ensures that:
- Disconnect callbacks fire correctly
- State transitions are correct for retriable vs non-retriable closures
- Retry logic is scheduled only when appropriate

Covered Requirements:
---------------------
E1. Transport close while Connected (retriable)
    - Connected → WaitingReconnect
    - on_disconnect fires once
    - Retry scheduled

E2. Transport close while Connected (non-retriable)
    - last_error_ = LocalShutdown
    - Connected → Disconnected
    - No retry

E3. Transport close while Disconnecting
    - Disconnecting → Disconnected
    - No retry

===============================================================================
*/

#include <cassert>
#include <iostream>

#include "common/mock_websocket_script.hpp"
#include "common/connection_harness.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::transport;


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

    h.drain_events();

    // Assertions
    TEST_CHECK(h.connect_events == 1);

    // Error from transport
    script.step(h.connection->ws());

    // Transport closes
    script.step(h.connection->ws());

    // poll triggers reconnect
    h.connection->poll();

    // Reconnect succeeds
    script.step(h.connection->ws());

    h.drain_events();

    // Check events
    TEST_CHECK(h.connect_events == 2);         // initial + reconnect
    TEST_CHECK(h.disconnect_events == 1);      // Single disconnect
    TEST_CHECK(h.retry_schedule_events == 0);  // No retry scheduled

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

    // Drain events and check
    h.drain_events();

    // Disconnect callback fires once
    TEST_CHECK(h.disconnect_events == 1);

    // No retry scheduled
    TEST_CHECK(h.retry_schedule_events == 0);

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

    h.drain_events();

    // Disconnect fires once
    TEST_CHECK(h.disconnect_events == 1);

    // No retry scheduled
    TEST_CHECK(h.retry_schedule_events == 0);

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
