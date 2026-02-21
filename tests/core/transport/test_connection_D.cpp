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

#include "common/connection_harness.hpp"
#include "common/mock_websocket_script.hpp"


// -----------------------------------------------------------------------------
// D1. Incoming message updates liveness and propagates payload
// -----------------------------------------------------------------------------
void test_message_dispatch_updates_liveness() {
    std::cout << "[TEST] Group D1: message dispatch updates liveness\n";
    WebSocketUnderTest::reset();

    using clock = std::chrono::steady_clock;

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .message("hello-world");

    telemetry::Connection telemetry;
    ConnectionUnderTest connection{g_ring, telemetry};

    // Open connection (does not run script yet)
    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    // Capture timestamp before message
    const auto before = connection.last_message_ts();

    // Step connect_ok
    script.step(connection.ws());

    // Step message (MockWebSocket pushes DataBlock into transport ring)
    script.step(connection.ws());

    // Poll connection to advance state
    connection.poll();

    // Pull message from transport data-plane
    auto* block = connection.peek_message();
    TEST_CHECK(block != nullptr);

    std::string received{block->data, block->size};

    // Release slot (mandatory)
    connection.release_message();

    // Payload must be forwarded exactly
    TEST_CHECK(received == "hello-world");

    // Liveness must be updated
    const auto after = connection.last_message_ts();
    TEST_CHECK(after > before);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// D2. Message dispatch ignored safely when no handler is registered
// -----------------------------------------------------------------------------
void test_message_dispatch_without_handler() {
    std::cout << "[TEST] Group D2: message dispatch without handler\n";
    WebSocketUnderTest::reset();

    using clock = std::chrono::steady_clock;

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .message("no-listener");

    telemetry::Connection telemetry;
    ConnectionUnderTest connection{g_ring, telemetry};

    // No on_message handler registered

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    const auto before = connection.last_message_ts();

    // Step connect_ok
    script.step(connection.ws());

    // Step message — must not crash
    script.step(connection.ws());

    // Advance state
    connection.poll();

    // Consume message (even if user has no handler)
    auto* block = connection.peek_message();
    TEST_CHECK(block != nullptr);

    // Release slot
    connection.release_message();

    // Liveness must still be updated
    const auto after = connection.last_message_ts();
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
