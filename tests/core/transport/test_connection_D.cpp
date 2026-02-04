/*
===============================================================================
 transport::Connection — Group D Unit Tests
===============================================================================

Scope:
------
These tests validate message propagation from the transport layer to
user-defined callbacks.

This group ensures that:
- Incoming WebSocket messages are forwarded verbatim to the user
- Message receipt updates liveness tracking
- Absence of a message handler is safe and does not suppress liveness updates

These tests use a fully deterministic MockWebSocket and explicit message
emission. No timing assumptions or polling heuristics are involved.

Covered Requirements:
---------------------
D1. Incoming message updates liveness
    - on_message invoked with exact payload
    - last_message_ts_ updated

D2. Message dispatch ignored when no handler
    - No crash
    - last_message_ts_ still updated

===============================================================================
*/

#include <cassert>
#include <iostream>
#include <string>
#include <chrono>

#include "wirekrak/core/transport/connection.hpp"
#include "common/mock_websocket.hpp"
#include "common/mock_websocket_script.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::transport;


// -----------------------------------------------------------------------------
// D1. Incoming message updates liveness and propagates payload
// -----------------------------------------------------------------------------
void test_message_dispatch_updates_liveness() {
    std::cout << "[TEST] Group D1: message dispatch updates liveness\n";
    test::MockWebSocket::reset();

    using clock = std::chrono::steady_clock;

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .message("hello-world");

    telemetry::Connection telemetry;
    Connection<test::MockWebSocket> connection{telemetry};

    std::string received;
    connection.on_message([&](std::string_view msg) {
        received = msg;
    });

    // Open connection (does not run script yet)
    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    // Capture timestamp before message
    const auto before =
        connection.get_last_message_ts().load(std::memory_order_relaxed);

    // Step connect_ok
    script.step(connection.ws());

    // Step message
    script.step(connection.ws());

    // Payload must be forwarded exactly
    TEST_CHECK(received == "hello-world");

    // Liveness must be updated
    const auto after = connection.get_last_message_ts().load(std::memory_order_relaxed);
    TEST_CHECK(after > before);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// D2. Message dispatch ignored safely when no handler is registered
// -----------------------------------------------------------------------------
void test_message_dispatch_without_handler() {
    std::cout << "[TEST] Group D2: message dispatch without handler\n";
    test::MockWebSocket::reset();

    using clock = std::chrono::steady_clock;

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .message("no-listener");

    telemetry::Connection telemetry;
    Connection<test::MockWebSocket> connection{telemetry};

    // No on_message handler registered

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    const auto before = connection.get_last_message_ts().load(std::memory_order_relaxed);

    // Step connect_ok
    script.step(connection.ws());

    // Step message — must not crash
    script.step(connection.ws());

    // Liveness must still be updated
    const auto after = connection.get_last_message_ts().load(std::memory_order_relaxed);
    TEST_CHECK(after > before);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_message_dispatch_updates_liveness();
    test_message_dispatch_without_handler();

    std::cout << "\n[GROUP D — MESSAGE DISPATCH TESTS PASSED]\n";
    return 0;
}
