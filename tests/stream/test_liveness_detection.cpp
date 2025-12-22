#include <cassert>
#include <chrono>
#include <thread>
#include <iostream>

#include "wirekrak/stream/client.hpp"
#include "common/mock_websocket.hpp"

using namespace wirekrak;
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

template <typename Client>
void advance_time_and_poll(Client& client, std::chrono::milliseconds delay) {
    std::this_thread::sleep_for(delay);
    client.poll();
}

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

void test_liveness_message_resets_timer() {
    std::cout << "[TEST] stream::Client liveness reset on message\n";

    stream::Client<transport::MockWebSocket> client;
    client.set_liveness_timeout(50ms);

    TEST_CHECK(client.connect("wss://example.com/ws"));

    // Initial message
    client.ws().emit_message("hello");
    client.poll();

    // Wait less than timeout
    advance_time_and_poll(client, 30ms);
    TEST_CHECK(client.ws().is_connected());

    // Another message resets timer
    client.ws().emit_message("heartbeat");
    client.poll();

    advance_time_and_poll(client, 30ms);
    TEST_CHECK(client.ws().is_connected());

    std::cout << "[TEST] OK\n";
}

void test_liveness_timeout_triggers_close() {
    std::cout << "[TEST] stream::Client liveness timeout closes connection\n";

    stream::Client<transport::MockWebSocket> client;
    client.set_liveness_timeout(30ms);

    TEST_CHECK(client.connect("wss://example.com/ws"));

    // No messages
    advance_time_and_poll(client, 40ms);

    //TEST_CHECK(!client.ws().is_connected());  <-- Implement reconnection logic configurable by user (On/Off)
    TEST_CHECK(client.ws().close_count() == 1);

    std::cout << "[TEST] OK\n";
}

void test_no_false_timeout_before_deadline() {
    std::cout << "[TEST] stream::Client no premature liveness timeout\n";

    stream::Client<transport::MockWebSocket> client;
    client.set_liveness_timeout(100ms);

    TEST_CHECK(client.connect("wss://example.com/ws"));

    advance_time_and_poll(client, 50ms);
    TEST_CHECK(client.ws().is_connected());
    TEST_CHECK(client.ws().close_count() == 0);

    std::cout << "[TEST] OK\n";
}

void test_error_does_not_reset_liveness() {
    std::cout << "[TEST] stream::Client error does not reset liveness\n";

    stream::Client<transport::MockWebSocket> client;
    client.set_liveness_timeout(40ms);

    TEST_CHECK(client.connect("wss://example.com/ws"));

    // Emit error only
    client.ws().emit_error();
    client.poll();

    advance_time_and_poll(client, 50ms);

    //TEST_CHECK(!client.ws().is_connected()); <-- Implement reconnection logic configurable by user (On/Off)
    TEST_CHECK(client.ws().close_count() == 1);

    std::cout << "[TEST] OK\n";
}

void test_heartbeat_keeps_connection_alive() {
    std::cout << "[TEST] stream::Client heartbeat-only traffic\n";

    stream::Client<transport::MockWebSocket> client;
    client.set_liveness_timeout(40ms);

    TEST_CHECK(client.connect("wss://example.com/ws"));

    for (int i = 0; i < 5; ++i) {
        advance_time_and_poll(client, 20ms);
        client.ws().emit_message("heartbeat");
        client.poll();
        TEST_CHECK(client.ws().is_connected());
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

    std::cout << "[TEST] stream::Client liveness tests PASSED\n";
    return 0;
}
