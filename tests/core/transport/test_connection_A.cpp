/*
===============================================================================
 transport::Connection — Group A Unit Tests
===============================================================================

Scope:
------
These tests validate *construction and lifecycle guarantees* of
wirekrak::core::transport::Connection<WS>.

This group intentionally avoids transport event sequencing and timing logic.
It focuses exclusively on:

- Correct initial state
- Safe behavior before open()
- RAII correctness and deterministic cleanup

These tests are:
- Fully deterministic
- Free of sleeps, timers, or polling heuristics
- Independent of reconnect, liveness, or protocol logic

Covered Requirements:
---------------------
A1. Default construction
    - Initial state is Disconnected
    - No callbacks are invoked
    - No transport instance is created implicitly

A2. Destructor closes transport
    - Transport is created via open()
    - Connection destruction closes the transport exactly once
    - No reconnection or duplicate close occurs

Non-Goals:
----------
- Transport error handling
- Reconnection logic
- Liveness detection
- URL parsing edge cases
- WebSocket protocol semantics

===============================================================================
*/

#include <cassert>
#include <iostream>
#include <string>

#include "common/connection_harness.hpp"


// -----------------------------------------------------------------------------
// Group A1: Default construction
// -----------------------------------------------------------------------------
void test_default_construction() {
    std::cout << "[TEST] Group A1: default construction\n";
    WebSocketUnderTest::reset();

    telemetry::Connection telemetry;
    ConnectionUnderTest connection{g_ring, telemetry};

    // Cannot send while disconnected
    TEST_CHECK(connection.send("ping") == false);

    // close() on a fresh connection must be safe and idempotent
    connection.close();

    // No transport should have been created implicitly
    TEST_CHECK(WebSocketUnderTest::close_count() == 0);
    TEST_CHECK(WebSocketUnderTest::error_count() == 0);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Group A2: Destructor closes transport
// -----------------------------------------------------------------------------
void test_destructor_closes_transport() {
    std::cout << "[TEST] Group A2: destructor closes transport\n";
    WebSocketUnderTest::reset();

    {
        telemetry::Connection telemetry;
        ConnectionUnderTest connection{g_ring, telemetry};

        // Open connection successfully
        TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

        // Sanity: transport must be connected
        TEST_CHECK(connection.ws().is_connected());
    } // Destructor must run here

    // Transport close must be invoked exactly once
    TEST_CHECK(WebSocketUnderTest::close_count() == 1);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_default_construction();
    test_destructor_closes_transport();

    std::cout << "\n[GROUP A — CONSTRUCTION & LIFECYCLE TESTS PASSED]\n";
    return 0;
}
