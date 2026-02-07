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
    TEST_CHECK(h.signals.size() == 4);
    TEST_CHECK(h.signals[0] == connection::Signal::Connected);
    TEST_CHECK(h.signals[1] == connection::Signal::Disconnected);
    TEST_CHECK(h.signals[2] == connection::Signal::RetryImmediate);
    TEST_CHECK(h.signals[3] == connection::Signal::Connected);

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
// I3. RetryScheduled is emitted only after immediate retry failure
// -----------------------------------------------------------------------------

void test_retry_scheduled_after_immediate_retry_failure() {
    std::cout << "[TEST] Group I3: RetryScheduled emitted after immediate retry failure\n";

    test::MockWebSocketScript script;
    script
        .connect_ok()
        .error(Error::RemoteClosed)
        .close();

    test::ConnectionHarness h;

    TEST_CHECK(h.connection->open("wss://example.com/ws") == Error::None);

    // Initial connect
    script.step(h.connection->ws());

    // Transport error + close
    script.step(h.connection->ws());
    script.step(h.connection->ws());

    // Force immediate reconnect attempt to fail
    h.connection->ws().set_next_connect_result(Error::ConnectionFailed);

    // poll triggers:
    //  - RetryImmediate
    //  - reconnect attempt
    //  - failure
    //  - RetryScheduled
    h.connection->poll();

    h.drain_signals();

    // Expected signal sequence:
    TEST_CHECK(h.signals.size() == 4);
    TEST_CHECK(h.signals[0] == connection::Signal::Connected);
    TEST_CHECK(h.signals[1] == connection::Signal::Disconnected);
    TEST_CHECK(h.signals[2] == connection::Signal::RetryImmediate);
    TEST_CHECK(h.signals[3] == connection::Signal::RetryScheduled);

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// Test entry point
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    test_disconnected_signal_precedes_reconnected_signal();
    test_connected_signal_not_duplicated_without_disconnect();
    test_retry_scheduled_after_immediate_retry_failure();
    return 0;
}
