/*
===============================================================================
 transport::Connection — Group C Unit Tests
===============================================================================

Scope:
------
These tests validate caller-facing semantics of Connection::send().

This group ensures that:
- send() only succeeds when the logical connection is established
- send() is safe to call in invalid states
- send() never touches the transport unless connected

These tests intentionally avoid inspecting internal timestamps or state.

Covered Requirements:
---------------------
C1. send() succeeds only when connected
    - Returns true

C2. send() fails when not connected
    - Disconnected / WaitingReconnect
    - Returns false

===============================================================================
*/

#include <cassert>
#include <iostream>
#include <string>

#include "common/harness/connection.hpp"


// -----------------------------------------------------------------------------
// C1. send() succeeds when connected
// -----------------------------------------------------------------------------
void test_send_when_connected() {
    std::cout << "[TEST] Group C1: send() succeeds when connected\n";
    WebSocketUnderTest::reset();

    telemetry::Connection telemetry;
    ConnectionUnderTest connection{g_ring, telemetry};

    // Establish connection
    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);
    TEST_CHECK(connection.ws().is_connected());

    // send() must succeed
    TEST_CHECK(connection.send("ping") == true);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// C2a. send() fails when Disconnected
// -----------------------------------------------------------------------------
void test_send_when_disconnected() {
    std::cout << "[TEST] Group C2a: send() fails when Disconnected\n";
    WebSocketUnderTest::reset();

    telemetry::Connection telemetry;
    ConnectionUnderTest connection{g_ring, telemetry};

    TEST_CHECK(connection.send("ping") == false);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// C2b. send() fails when WaitingReconnect
// -----------------------------------------------------------------------------
void test_send_when_waiting_reconnect() {
    std::cout << "[TEST] Group C2b: send() fails when WaitingReconnect\n";
    WebSocketUnderTest::reset();

    // Force retriable failure
    WebSocketUnderTest::set_next_connect_result(Error::ConnectionFailed);

    telemetry::Connection telemetry;
    ConnectionUnderTest connection{g_ring, telemetry};

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::ConnectionFailed);

    // Must not be allowed to send
    TEST_CHECK(connection.send("ping") == false);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_send_when_connected();
    test_send_when_disconnected();
    test_send_when_waiting_reconnect();

    std::cout << "\n[GROUP C — SEND() SEMANTICS TESTS PASSED]\n";
    return 0;
}
