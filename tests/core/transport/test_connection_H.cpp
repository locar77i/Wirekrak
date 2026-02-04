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

#include "common/mock_websocket_script.hpp"
#include "common/connection_harness.hpp"
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

        test::MockWebSocketScript script;
        script
            .connect_ok()
            .error(error)
            .close()
            .connect_ok(); // reconnect succeeds

        test::ConnectionHarness h;

        TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);
        script.step(h.connection->ws()); // initial connect

        h.drain_events();

        // First connect event
        TEST_CHECK(h.connect_events == 1);

        // inject error + close
        script.step(h.connection->ws());
        script.step(h.connection->ws());

        // poll triggers immediate retry
        h.connection->poll();
        script.step(h.connection->ws());

        h.drain_events();

        // Check events
        TEST_CHECK(h.connect_events == 2);
        TEST_CHECK(h.disconnect_events == 1);
        TEST_CHECK(h.retry_schedule_events == 0); 

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

        test::MockWebSocketScript script;
        script
            .connect_ok()
            .error(error)
            .close();

        test::ConnectionHarness h;

        TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);
        script.step(h.connection->ws()); // initial connect

        h.drain_events();

        // First connect event
        TEST_CHECK(h.connect_events == 1);

        // inject error + close
        script.step(h.connection->ws());
        script.step(h.connection->ws());

        // poll should NOT retry
        h.connection->poll();
        h.connection->poll();

        h.drain_events();

        // Check events
        TEST_CHECK(h.connect_events == 1);
        TEST_CHECK(h.disconnect_events == 1);
        TEST_CHECK(h.retry_schedule_events == 0);

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
