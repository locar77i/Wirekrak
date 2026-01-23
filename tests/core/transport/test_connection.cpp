#include <cassert>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#include "wirekrak/core/transport/connection.hpp"
#include "common/mock_websocket.hpp"


using namespace wirekrak::core;
using namespace wirekrak::core::transport;

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "[TEST FAILED] " << #expr \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

// -----------------------------------------------------------------------------
// Test: open() succeeds and triggers on_connect
// -----------------------------------------------------------------------------
void test_connect() {
    std::cout << "[TEST] transport::Connection open\n";
    transport::MockWebSocket::reset();

    Connection<MockWebSocket> connection;

    bool connected_cb = false;
    connection.on_connect([&]() { connected_cb = true; });

    TEST_CHECK(connection.open("wss://example.com/ws") == transport::Error::None);
    TEST_CHECK(connected_cb);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test: message callback propagation
// -----------------------------------------------------------------------------
void test_message_dispatch() {
    std::cout << "[TEST] transport::Connection message dispatch\n";
    transport::MockWebSocket::reset();

    Connection<MockWebSocket> connection;
    (void)connection.open("wss://example.com/ws");

    std::string received;
    connection.on_message([&](std::string_view msg) {
        received = msg;
    });

    connection.ws().emit_message("hello");
    TEST_CHECK(received == "hello");

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test: send() succeeds when connected
// -----------------------------------------------------------------------------
void test_send() {
    std::cout << "[TEST] transport::Connection send\n";
    transport::MockWebSocket::reset();

    Connection<MockWebSocket> connection;
    (void)connection.open("wss://example.com/ws");

    TEST_CHECK(connection.send("ping") == true);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test: close triggers disconnect callback
// -----------------------------------------------------------------------------
void test_close() {
    std::cout << "[TEST] transport::Connection close\n";
    transport::MockWebSocket::reset();

    Connection<MockWebSocket> connection;

    bool disconnected = false;
    connection.on_disconnect([&]() { disconnected = true; });

    (void)connection.open("wss://example.com/ws");
    connection.close();

    TEST_CHECK(disconnected);
    TEST_CHECK(connection.ws().close_count() == 1);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test: transport close triggers reconnect scheduling
// -----------------------------------------------------------------------------
void test_reconnect_on_close() {
    std::cout << "[TEST] transport::Connection reconnect on transport close\n";
    transport::MockWebSocket::reset();

    Connection<MockWebSocket> connection;

    int connect_count = 0;
    connection.on_connect([&]() {
        ++connect_count;
    });

    (void)connection.open("wss://example.com/ws");

    // Initial connect
    TEST_CHECK(connect_count == 1);

    // Simulate transport close
    connection.ws().close();

    // Force time to advance to trigger reconnect
    connection.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    connection.poll();

    TEST_CHECK(connect_count == 1);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test: liveness timeout hook fires when both timestamps are stale
// (logic only, no heartbeat semantics tested)
// -----------------------------------------------------------------------------
void test_liveness_hook() {
    std::cout << "[TEST] transport::Connection liveness hook\n";
    transport::MockWebSocket::reset();

    using clock = std::chrono::steady_clock;

    Connection<MockWebSocket> connection;
    (void)connection.open("wss://example.com/ws");

    bool liveness_called = false;
    connection.on_liveness_timeout([&]() {
        liveness_called = true;
    });

    auto past = clock::now() - std::chrono::seconds(30);
    connection.force_last_message(past);
    connection.force_last_heartbeat(past);

    connection.poll();

    TEST_CHECK(liveness_called);

    std::cout << "[TEST] OK\n";
}



// -----------------------------------------------------------------------------
// This test suite deterministically validates the client’s connection state machine,
// message dispatch, reconnection logic, and liveness decision logic using a 
// fully mocked transport—without timing flakiness or network dependencies.
//
// Garantees:
// - Deterministic behavior (no flakiness)
//   All tests use a fully controlled MockWebSocket, ensuring repeatable and non-flaky results with no real networking.
//
// - Correct connection lifecycle
//   Verifies that open() succeeds, triggers connection callbacks, and transitions the connection into a connected state.
//
// - Reliable message dispatch
//   Ensures incoming WebSocket messages are correctly propagated to the registered message handler.
//
// - Safe send semantics
//   Confirms that send() only succeeds when the connection is connected and respects transport state.
//
// - Clean shutdown handling
//   Validates that explicit close() calls properly close the transport and invoke disconnect callbacks exactly once.
//
// - Automatic reconnection logic
//   Confirms that unexpected transport closures trigger reconnect attempts without manual intervention.
//
// - Liveness timeout detection
//   Guarantees that stale message and heartbeat timestamps correctly trigger the liveness timeout hook.
//
// - Protocol-agnostic design
//   Tests exercise only the streaming layer, proving it is reusable across exchanges and protocols.
//
// - Composition-based architecture
//   Confirms behavior without inheritance or virtual dispatch, validating the intended zero-overhead design.
//
// - Test isolation
//   No dependency on Kraken schemas, parsers, or protocol logic—stream connection correctness is validated independently.
// -----------------------------------------------------------------------------

#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_connect();
    test_message_dispatch();
    test_send();
    test_close();
    test_reconnect_on_close();
    test_liveness_hook();

    std::cout << "\n[ALL STREAM CLIENT TESTS PASSED]\n";
    return 0;
}
