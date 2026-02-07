/*
===============================================================================
 transport::Connection — Group I Unit Tests
 Observable signal ordering & guarantees
===============================================================================

Scope:
------
These tests validate **externally observable connection signals**, not internal
FSM mechanics or transport implementation details.

They ensure:

- Correct ordering of edge-triggered connection::Signal emissions
- No duplicate Connected signals without an intervening Disconnected
- RetryScheduled is emitted only when a retry cycle is actually scheduled
- Reconnect success does NOT emit RetryScheduled
- All observable behavior is poll-driven and deterministic

IMPORTANT TESTING RULE:
-----------------------
Reconnect attempts and signal emission occur synchronously inside poll().

All reconnect outcomes MUST be scripted via MockWebSocketScript
*before* calling poll().

These tests do NOT:
- Inspect internal state
- Depend on timing or background threads
- Assume delivery of any signal (signals are best-effort)

They validate only what a consumer can **observe** via poll_signal().

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
// I1. Disconnected signal is observed before reconnect Connected signal
// -----------------------------------------------------------------------------

void test_disconnected_signal_precedes_reconnected_signal() {
    std::cout << "[TEST] Group I1: Disconnected signal precedes reconnect Connected signal\n";

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

    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 2);
    TEST_CHECK(h.disconnect_signals = 1);
    TEST_CHECK(h.retry_schedule_signals == 0);
    TEST_CHECK(h.signals.size() == 3);
    TEST_CHECK(h.signals[0] == connection::Signal::Connected);
    TEST_CHECK(h.signals[1] == connection::Signal::Disconnected);
    TEST_CHECK(h.signals[2] == connection::Signal::Connected);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// I2. Connected signal is never emitted twice without Disconnected
// -----------------------------------------------------------------------------

void test_connected_signal_not_duplicated_without_disconnect() {
    std::cout << "[TEST] Group I2: Connected signal is never emitted twice without Disconnected\n";

    test::MockWebSocketScript script;
    script.connect_ok();

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);
    script.step(h.connection->ws());

    // Multiple polls must not re-emit Connected without an intervening Disconnected
    for (int i = 0; i < 10; ++i) {
        h.connection->poll();
    }

    h.drain_signals();

    // Check signals
    TEST_CHECK(h.connect_signals == 1);
    TEST_CHECK(h.disconnect_signals == 0);
    TEST_CHECK(h.retry_schedule_signals == 0);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// I3. Retry callback fires only when a retry cycle is scheduled
//     (not on immediate reconnect success)
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
    test_disconnected_signal_precedes_reconnected_signal();
    test_connected_signal_not_duplicated_without_disconnect();
    test_on_retry_before_scheduled_retry();
    return 0;
}
