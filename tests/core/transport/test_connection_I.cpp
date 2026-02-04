/*
===============================================================================
 transport::Connection — Group I Unit Tests
 Callback ordering & guarantees
===============================================================================

Scope:
------
These tests validate *observable callback guarantees*, not internal mechanics.

They ensure:
- Correct ordering between disconnect, retry, and reconnect callbacks
- No duplicate connect notifications
- Retry callbacks fire only when a reconnect is scheduled (not on immediate success)

IMPORTANT TESTING RULE:
-----------------------
Reconnect attempts occur synchronously inside poll().

All reconnect outcomes MUST be scripted via MockWebSocketScript
*before* calling poll().

===============================================================================
*/

#include <vector>
#include <string>
#include <iostream>

#include "common/mock_websocket_script.hpp"
#include "common/connection_harness.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::transport;

// -----------------------------------------------------------------------------
// I1. on_disconnect fires before reconnect
// -----------------------------------------------------------------------------

void test_on_disconnect_before_reconnect() {
    std::cout << "[TEST] Group I1: on_disconnect fires before reconnect\n";

   test::MockWebSocketScript script;
    script
        .connect_ok()
        .error(Error::RemoteClosed)
        .close()
        .connect_ok(); // reconnect succeeds

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);
    script.step(h.connection->ws()); // initial connect

    // Inject error + close
    script.step(h.connection->ws());
    script.step(h.connection->ws());

    // poll triggers reconnect
    h.connection->poll();
    script.step(h.connection->ws());

    h.drain_events();

    // Check events
    TEST_CHECK(h.connect_events == 2);
    TEST_CHECK(h.disconnect_events = 1);
    TEST_CHECK(h.retry_schedule_events == 0);
    TEST_CHECK(h.events.size() == 3);
    TEST_CHECK(h.events[0] == TransitionEvent::Connected);
    TEST_CHECK(h.events[1] == TransitionEvent::Disconnected);
    TEST_CHECK(h.events[2] == TransitionEvent::Connected);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// I2. on_connect never fires twice without disconnect
// -----------------------------------------------------------------------------

void test_on_connect_not_duplicated() {
    std::cout << "[TEST] Group I2: on_connect never fires twice without disconnect\n";

    test::MockWebSocketScript script;
    script.connect_ok();

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);
    script.step(h.connection->ws());

    // Multiple polls must not re-emit on_connect
    for (int i = 0; i < 10; ++i) {
        h.connection->poll();
    }

    h.drain_events();

    // Check events
    TEST_CHECK(h.connect_events == 1);
    TEST_CHECK(h.disconnect_events == 0);
    TEST_CHECK(h.retry_schedule_events == 0);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// I3. on_retry invoked before scheduled retry
// -----------------------------------------------------------------------------

void test_on_retry_before_scheduled_retry() {
    std::cout << "[TEST] Group I3: on_retry invoked before scheduled retry\n";

    test::MockWebSocket::reset();

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .error(Error::RemoteClosed)
        .close();

    telemetry::Connection telemetry;
    Connection<test::MockWebSocket> connection{telemetry};

    int retry_calls = 0;
    int observed_attempt = -1;

    connection.on_retry([&](const RetryContext& ctx) {
        ++retry_calls;
        observed_attempt = ctx.attempt;
    });

    TEST_CHECK(connection.open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(connection.ws());

    // Transport error + close
    script.step(connection.ws());
    script.step(connection.ws());

    // IMPORTANT:
    // Program reconnect failure *before* poll()
    connection.ws().set_next_connect_result(Error::ConnectionFailed);

    // poll triggers reconnect attempt → failure → retry scheduling
    connection.poll();

    TEST_CHECK(retry_calls == 1);
    TEST_CHECK(observed_attempt == 2);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test entry point
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    test_on_disconnect_before_reconnect();
    test_on_connect_not_duplicated();
    test_on_retry_before_scheduled_retry();
    return 0;
}
