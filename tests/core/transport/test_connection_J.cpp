/*
===============================================================================
 transport::Connection — Group J Unit Tests
 Shutdown & Destructor Guarantees
===============================================================================

Scope:
------
These tests validate deterministic shutdown behavior.

They ensure:
- close() performs a clean, one-time shutdown
- Destructor closes the transport safely
- No retries or callbacks occur after shutdown
- close() is idempotent

These tests MUST NOT involve reconnection scripts beyond initial setup.

===============================================================================
*/

#include <iostream>
#include <memory>

#include "wirekrak/core/transport/connection.hpp"
#include "common/mock_websocket.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::transport;

// -----------------------------------------------------------------------------
// J1. close() performs graceful shutdown
// -----------------------------------------------------------------------------

void test_close_graceful_shutdown() {
    std::cout << "[TEST] Group J1: close() performs graceful shutdown\n";

    test::MockWebSocket::reset();

    telemetry::Connection telemetry;
    Connection<test::MockWebSocket> connection{telemetry};

    int disconnect_calls = 0;

    connection.on_disconnect([&]() {
        ++disconnect_calls;
    });

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    connection.close();

    TEST_CHECK(disconnect_calls == 1);
    TEST_CHECK(connection.ws().close_count() == 1);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// J2. Destructor closes active transport
// -----------------------------------------------------------------------------

void test_destructor_closes_transport() {
    std::cout << "[TEST] Group J2: destructor closes transport\n";

    test::MockWebSocket::reset();

    int close_calls = 0;

    {
        telemetry::Connection telemetry;
        Connection<test::MockWebSocket> connection{telemetry};

        TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

        close_calls = connection.ws().close_count();
    }

    // Destructor must have closed transport
    TEST_CHECK(test::MockWebSocket::close_count() == 1);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// J3. close() is idempotent
// -----------------------------------------------------------------------------

void test_close_idempotent() {
    std::cout << "[TEST] Group J3: close() is idempotent\n";

    test::MockWebSocket::reset();

    telemetry::Connection telemetry;
    Connection<test::MockWebSocket> connection{telemetry};

    int disconnect_calls = 0;

    connection.on_disconnect([&]() {
        ++disconnect_calls;
    });

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    connection.close();
    connection.close();
    connection.close();

    TEST_CHECK(disconnect_calls == 1);
    TEST_CHECK(connection.ws().close_count() == 1);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// J4. Destructor does not schedule reconnect
// -----------------------------------------------------------------------------

void test_destructor_no_reconnect() {
    std::cout << "[TEST] Group J4: destructor does not schedule reconnect\n";

    test::MockWebSocket::reset();

    int retry_calls = 0;
    int connect_calls = 0;

    {
        telemetry::Connection telemetry;
        Connection<test::MockWebSocket> connection{telemetry};

        connection.on_retry([&](const RetryContext&) {
            ++retry_calls;
        });

        connection.on_connect([&]() {
            ++connect_calls;
        });

        TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

        // Simulate transport failure
        connection.ws().emit_error(Error::RemoteClosed);
        connection.ws().close();

        // Do NOT poll — destructor must prevent retries
    }

    TEST_CHECK(retry_calls == 0);
    TEST_CHECK(connect_calls == 1); // only initial connect

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test entry point
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    test_close_graceful_shutdown();
    test_destructor_closes_transport();
    test_close_idempotent();
    test_destructor_no_reconnect();

    std::cout << "\n[ALL GROUP J TESTS PASSED]\n";
    return 0;
}
