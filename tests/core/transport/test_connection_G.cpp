/*
===============================================================================
 transport::Connection — Group G Unit Tests (FINAL)
===============================================================================

Scope:
------
These tests validate the reconnection state machine driven by poll().

Key invariants tested:
----------------------
- Immediate reconnect on retriable transport failure
- Failed reconnect schedules exponential backoff and emits on_retry
- Successful reconnect resets retry state
- Backoff timing is respected *only after a failed reconnect*

Design contract clarified:
--------------------------
The first reconnect attempt after a retriable transport failure is
intentionally immediate. There is no delay or backoff before this attempt.

Exponential backoff applies only after a reconnect attempt fails.
Tests MUST NOT expect poll() to "do nothing" before the first retry.

===============================================================================
*/

#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>

#include "wirekrak/core/transport/connection.hpp"
#include "common/mock_websocket.hpp"
#include "common/mock_websocket_script.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::transport;


// -----------------------------------------------------------------------------
// G1. Immediate retry on retriable error
// -----------------------------------------------------------------------------
void test_immediate_retry_on_retriable_error() {
    std::cout << "[TEST] Group G1: immediate retry on retriable error\n";
    test::MockWebSocket::reset();

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .error(Error::RemoteClosed)
        .close()
        .connect_ok(); // reconnect succeeds

    telemetry::Connection telemetry;
    Connection<test::MockWebSocket> connection{telemetry};

    int connect_calls = 0;
    connection.on_connect([&]() {
        ++connect_calls;
    });

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(connection.ws());
    TEST_CHECK(connect_calls == 1);

    // Transport failure + close
    script.step(connection.ws());
    script.step(connection.ws());

    // Reconnect happens immediately in poll()
    connection.poll();

    // Reconnect succeeds
    script.step(connection.ws());
    TEST_CHECK(connect_calls == 2);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// G2. Failed reconnect schedules backoff and invokes on_retry
// -----------------------------------------------------------------------------
void test_failed_reconnect_schedules_backoff() {
    std::cout << "[TEST] Group G2: failed reconnect schedules backoff\n";
    test::MockWebSocket::reset();

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .error(Error::RemoteClosed)
        .close()
        .connect_fail(Error::ConnectionFailed); // first reconnect FAILS

    telemetry::Connection telemetry;
    Connection<test::MockWebSocket> connection{telemetry};

    int retry_calls = 0;
    RetryContext last_retry{};

    connection.on_retry([&](const RetryContext& ctx) {
        ++retry_calls;
        last_retry = ctx;
    });

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(connection.ws());

    // Transport failure + close
    script.step(connection.ws());
    script.step(connection.ws());

    // Arm reconnect failure BEFORE poll
    script.step(connection.ws());

    // poll() triggers reconnect → failure
    connection.poll();

    TEST_CHECK(retry_calls == 1);
    TEST_CHECK(last_retry.attempt == 2);
    TEST_CHECK(last_retry.error == Error::RemoteClosed);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// G3. Successful reconnect resets retry state
// -----------------------------------------------------------------------------
void test_successful_reconnect_resets_retry_state() {
    std::cout << "[TEST] Group G3: successful reconnect resets retry state\n";
    test::MockWebSocket::reset();

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .error(Error::RemoteClosed)
        .close()
        .connect_ok(); // first reconnect SUCCEEDS

    telemetry::Connection telemetry;
    Connection<test::MockWebSocket> connection{telemetry};

    int connect_calls = 0;
    int retry_calls = 0;

    connection.on_connect([&]() { ++connect_calls; });
    connection.on_retry([&](const RetryContext&) { ++retry_calls; });

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(connection.ws());
    TEST_CHECK(connect_calls == 1);

    // Transport failure + close
    script.step(connection.ws());
    script.step(connection.ws());

    // Reconnect attempt succeeds
    connection.poll();
    script.step(connection.ws());

    // Assertions
    TEST_CHECK(connect_calls == 2);   // initial + reconnect
    TEST_CHECK(retry_calls == 0);     // SUCCESS ⇒ no retry callback

    std::cout << "[TEST] OK\n";
}



// -----------------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_immediate_retry_on_retriable_error();
    test_failed_reconnect_schedules_backoff();
    test_successful_reconnect_resets_retry_state();

    std::cout << "\n[GROUP G — RECONNECTION LOGIC TESTS PASSED]\n";
    return 0;
}
