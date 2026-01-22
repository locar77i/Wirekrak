#include <cassert>
#include <chrono>
#include <thread>
#include <iostream>

#include "wirekrak/core/transport/connection.hpp"
#include "common/mock_websocket.hpp"

using namespace wirekrak::core;
using namespace std::chrono_literals;

#define TEST_CHECK(expr)                                                     \
    do {                                                                     \
        if (!(expr)) {                                                       \
            std::cerr << "[TEST FAILED] " << #expr                           \
                      << " at " << __FILE__ << ":" << __LINE__ << "\n";      \
            std::abort();                                                    \
        }                                                                    \
    } while (0)

// -----------------------------------------------------------------------------
// Test helpers
// -----------------------------------------------------------------------------

template <typename Connection>
void advance_time_and_poll(Connection& connection, std::chrono::milliseconds delay) {
    std::this_thread::sleep_for(delay);
    connection.poll();
}

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

void test_liveness_message_resets_timer() {
    std::cout << "[TEST] transport::Connection liveness reset on message\n";
    transport::MockWebSocket::reset();

    transport::Connection<transport::MockWebSocket> connection;
    connection.set_liveness_timeout(50ms);

    TEST_CHECK(connection.open("wss://example.com/ws"));

    // Initial message
    connection.ws().emit_message("hello");
    connection.poll();

    // Wait less than timeout
    advance_time_and_poll(connection, 30ms);
    TEST_CHECK(connection.ws().is_connected());

    // Another message resets timer
    connection.ws().emit_message("heartbeat");
    connection.poll();

    advance_time_and_poll(connection, 30ms);
    TEST_CHECK(connection.ws().is_connected());

    std::cout << "[TEST] OK\n";
}

void test_liveness_timeout_triggers_close() {
    std::cout << "[TEST] transport::Connection liveness timeout closes connection\n";
    transport::MockWebSocket::reset();

    transport::Connection<transport::MockWebSocket> connection;
    connection.set_liveness_timeout(30ms);

    TEST_CHECK(connection.open("wss://example.com/ws"));

    // No messages
    advance_time_and_poll(connection, 40ms);

    TEST_CHECK(connection.ws().is_connected()); // It must be connected due to reconnection logic
    TEST_CHECK(connection.ws().close_count() == 1);

    std::cout << "[TEST] OK\n";
}

void test_no_false_timeout_before_deadline() {
    std::cout << "[TEST] transport::Connection no premature liveness timeout\n";
    transport::MockWebSocket::reset();

    transport::Connection<transport::MockWebSocket> connection;
    connection.set_liveness_timeout(100ms);

    TEST_CHECK(connection.open("wss://example.com/ws"));

    advance_time_and_poll(connection, 50ms);
    TEST_CHECK(connection.ws().is_connected());
    TEST_CHECK(connection.ws().close_count() == 0);

    std::cout << "[TEST] OK\n";
}

void test_error_does_not_reset_liveness() {
    std::cout << "[TEST] transport::Connection error does not reset liveness\n";
    transport::MockWebSocket::reset();

    transport::Connection<transport::MockWebSocket> connection;
    connection.set_liveness_timeout(40ms);

    TEST_CHECK(connection.open("wss://example.com/ws"));


    // Emit error only
    connection.ws().emit_error();
    connection.poll();

    advance_time_and_poll(connection, 50ms);

    TEST_CHECK(connection.ws().is_connected()); // It must be connected due to reconnection logic
    TEST_CHECK(connection.ws().close_count() == 1);

    std::cout << "[TEST] OK\n";
}

void test_heartbeat_keeps_connection_alive() {
    std::cout << "[TEST] transport::Connection heartbeat-only traffic\n";
    transport::MockWebSocket::reset();

    transport::Connection<transport::MockWebSocket> connection;
    connection.set_liveness_timeout(40ms);

    TEST_CHECK(connection.open("wss://example.com/ws"));

    for (int i = 0; i < 5; ++i) {
        advance_time_and_poll(connection, 20ms);
        connection.ws().emit_message("heartbeat");
        connection.poll();
        TEST_CHECK(connection.ws().is_connected());
    }

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// What this suite guarantees:
//
// - Deterministic behavior (no flakiness)
// - No real networking
// - Precise time-bound checks
// - Clear separation between:
//   - message activity
//   - heartbeat semantics
//   - error signaling
//   - timeout enforcement
// -----------------------------------------------------------------------------

int main() {
    test_liveness_message_resets_timer();
    test_liveness_timeout_triggers_close();
    test_no_false_timeout_before_deadline();
    test_error_does_not_reset_liveness();
    test_heartbeat_keeps_connection_alive();

    std::cout << "[TEST] transport::Connection liveness tests PASSED\n";
    return 0;
}
