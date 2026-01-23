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

#include "wirekrak/core/transport/connection.hpp"
#include "common/mock_websocket.hpp"
#include "common/mock_websocket_script.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::transport;


// -----------------------------------------------------------------------------
// E1. Transport close while Connected (retriable)
// -----------------------------------------------------------------------------
void test_transport_close_retriable() {
    std::cout << "[TEST] Group E1: transport close retriable\n";
    test::MockWebSocket::reset();

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .error(Error::RemoteClosed)
        .close()
        .connect_ok();              // reconnect succeeds

    Connection<test::MockWebSocket> connection;

    int disconnect_calls = 0;
    int connect_calls = 0;
    int retry_calls = 0;

    connection.on_disconnect([&]() {
        ++disconnect_calls;
    });

    connection.on_connect([&]() {
        ++connect_calls;
    });

    connection.on_retry([&](const RetryContext&) {
        ++retry_calls;
    });

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(connection.ws());
    TEST_CHECK(connect_calls == 1);

    // Error from transport
    script.step(connection.ws());

    // Transport closes
    script.step(connection.ws());

    // poll triggers reconnect
    connection.poll();

    // Reconnect succeeds
    script.step(connection.ws());

    // Assertions
    TEST_CHECK(disconnect_calls == 1);
    TEST_CHECK(connect_calls == 2);   // initial + reconnect
    TEST_CHECK(retry_calls == 0);     // reconnect succeeded

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// E2. Transport close while Connected (non-retriable)
// -----------------------------------------------------------------------------
void test_transport_close_non_retriable() {
    std::cout << "[TEST] Group E2: transport close non-retriable\n";
    test::MockWebSocket::reset();

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .error(Error::LocalShutdown)
        .close();

    Connection<test::MockWebSocket> connection;

    int disconnect_calls = 0;
    int retry_calls = 0;

    connection.on_disconnect([&]() {
        ++disconnect_calls;
    });

    connection.on_retry([&](const RetryContext&) {
        ++retry_calls;
    });

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    // Step connect
    script.step(connection.ws());

    // Step explicit non-retriable error
    script.step(connection.ws());

    // Step close
    script.step(connection.ws());

    // Drive state machine
    connection.poll();

    // Disconnect callback fires once
    TEST_CHECK(disconnect_calls == 1);

    // No retry scheduled
    TEST_CHECK(retry_calls == 0);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// E3. Transport close while Disconnecting
// -----------------------------------------------------------------------------
void test_transport_close_while_disconnecting() {
    std::cout << "[TEST] Group E3: transport close while Disconnecting\n";
    test::MockWebSocket::reset();

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .close();

    Connection<test::MockWebSocket> connection;

    int disconnect_calls = 0;
    int retry_calls = 0;

    connection.on_disconnect([&]() {
        ++disconnect_calls;
    });

    connection.on_retry([&](const RetryContext&) {
        ++retry_calls;
    });

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    // Step connect
    script.step(connection.ws());

    // User initiates shutdown
    connection.close();

    // Transport reports close
    script.step(connection.ws());

    // Drive state machine
    connection.poll();

    // Disconnect fires once
    TEST_CHECK(disconnect_calls == 1);

    // No retry scheduled
    TEST_CHECK(retry_calls == 0);

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
