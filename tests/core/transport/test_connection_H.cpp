/*
===============================================================================
 transport::Connection — Group H Unit Tests (FINAL)
===============================================================================

Scope:
------
These tests validate the retry policy decision logic of transport::Connection.

Focus:
------
- Whether a given transport error is classified as retriable or non-retriable
- Independent of reconnection mechanics, timing, or backoff behavior

Out of scope:
-------------
- State machine transitions
- Backoff timing
- Transport creation / destruction
- Callback ordering

Method:
-------
Each test injects a specific transport error, forces transport closure,
and observes whether the connection enters WaitingReconnect or Disconnected.

===============================================================================
*/

#include <iostream>
#include <vector>

#include "wirekrak/core/transport/connection.hpp"
#include "common/mock_websocket.hpp"
#include "common/mock_websocket_script.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::transport;


// -----------------------------------------------------------------------------
// H1. Retriable errors trigger retry
// -----------------------------------------------------------------------------
void test_retriable_errors_trigger_retry() {
    std::cout << "[TEST] Group H1: retriable errors trigger retry\n";

    const std::vector<Error> retriable_errors = {
        Error::ConnectionFailed,
        Error::HandshakeFailed,
        Error::Timeout,
        Error::RemoteClosed,
        Error::TransportFailure
    };

    for (auto error : retriable_errors) {
        test::MockWebSocket::reset();

        test::MockWebSocketScript script;
        script
            .connect_ok()
            .error(error)
            .close()
            .connect_ok(); // reconnect succeeds

        telemetry::Connection telemetry;
        Connection<test::MockWebSocket> connection{telemetry};

        int connect_calls = 0;
        connection.on_connect([&]() { ++connect_calls; });

        TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);
        script.step(connection.ws()); // initial connect
        TEST_CHECK(connect_calls == 1);

        // inject error + close
        script.step(connection.ws());
        script.step(connection.ws());

        // poll triggers immediate retry
        connection.poll();
        script.step(connection.ws());

        TEST_CHECK(connect_calls == 2);

        std::cout << "  ✓ retriable: " << to_string(error) << "\n";
    }

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// H2. Non-retriable errors never retry
// -----------------------------------------------------------------------------
void test_non_retriable_errors_never_retry() {
    std::cout << "[TEST] Group H2: non-retriable errors never retry\n";

    const std::vector<Error> non_retriable_errors = {
        Error::InvalidUrl,
        Error::InvalidState,
        Error::ProtocolError,
        Error::Cancelled,
        Error::LocalShutdown
    };

    for (auto error : non_retriable_errors) {
        test::MockWebSocket::reset();

        test::MockWebSocketScript script;
        script
            .connect_ok()
            .error(error)
            .close();

        telemetry::Connection telemetry;
        Connection<test::MockWebSocket> connection{telemetry};

        int connect_calls = 0;
        connection.on_connect([&]() { ++connect_calls; });

        TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);
        script.step(connection.ws()); // initial connect
        TEST_CHECK(connect_calls == 1);

        // inject error + close
        script.step(connection.ws());
        script.step(connection.ws());

        // poll should NOT retry
        connection.poll();
        connection.poll();

        TEST_CHECK(connect_calls == 1);

        std::cout << "  ✓ non-retriable: " << to_string(error) << "\n";
    }

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_retriable_errors_trigger_retry();
    test_non_retriable_errors_never_retry();

    std::cout << "\n[ALL GROUP H TESTS PASSED]\n";
    return 0;
}
