/*
===============================================================================
 protocol::kraken::Session - Group D Epoch & Replay Stress Tests
===============================================================================

Scope:
------
Validate transport epoch monotonicity and replay idempotency under stress.

Covered:
D1 Epoch strictly increases across reconnects
D2 Replay fires only once per epoch
D3 Multiple reconnects do not duplicate intent
D4 Replay convergence after repeated reconnects

These tests validate:
- Transport epoch monotonicity
- Replay idempotency
- No intent duplication
- Deterministic convergence

===============================================================================
*/

#include <iostream>
#include <string>

#include "common/harness/session.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::protocol::kraken::test;

// ----------------------------------------------------------------------------
// D1 Epoch strictly increases
// ----------------------------------------------------------------------------

void test_epoch_monotonicity() {
    std::cout << "[TEST] D1 Epoch monotonicity\n";

    SessionHarness h;
    h.connect();

    uint64_t e1 = h.session.transport_epoch();
    TEST_CHECK(e1 == 1);

    h.force_reconnect();
    h.wait_for_epoch(2);
    uint64_t e2 = h.session.transport_epoch();
    TEST_CHECK(e2 == 2);

    h.force_reconnect();
    h.wait_for_epoch(3);
    uint64_t e3 = h.session.transport_epoch();
    TEST_CHECK(e3 == 3);

    TEST_CHECK(e3 > e2 && e2 > e1);

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// D2 Replay fires only once per epoch
// ----------------------------------------------------------------------------

void test_replay_once_per_epoch() {
    std::cout << "[TEST] D2 Replay fires only once per epoch\n";

    SessionHarness h;
    h.connect();

    auto id = h.subscribe_trade("BTC/USD");
    h.confirm_trade_subscription(id, "BTC/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);

    h.force_reconnect();
    h.wait_for_epoch(2);

    // Replay must have created exactly one pending request
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 1);

    // Poll repeatedly - replay must NOT fire again
    for (int i = 0; i < 5; ++i)
        h.drain();

    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 1);

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// D3 Multiple reconnects do not duplicate intent
// ----------------------------------------------------------------------------

void test_no_duplicate_replay_across_epochs() {
    std::cout << "[TEST] D3 No duplicate replay across epochs\n";

    SessionHarness h;
    h.connect();

    auto id = h.subscribe_trade("BTC/USD");
    h.confirm_trade_subscription(id, "BTC/USD");

    for (int i = 2; i <= 5; ++i) {
        h.force_reconnect();
        h.wait_for_epoch(i);

        TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 1);

        h.confirm_trade_subscription(id, "BTC/USD");

        TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
        TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);
    }

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// D4 Repeated reconnect convergence stress
// ----------------------------------------------------------------------------

void test_reconnect_stress_convergence() {
    std::cout << "[TEST] D4 Reconnect convergence stress\n";

    SessionHarness h;
    h.connect();

    auto id1 = h.subscribe_trade("BTC/USD");
    auto id2 = h.subscribe_trade("ETH/USD");

    h.confirm_trade_subscription(id1, "BTC/USD");
    h.confirm_trade_subscription(id2, "ETH/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 2);

    for (int i = 2; i <= 6; ++i) {
        h.force_reconnect();
        h.wait_for_epoch(i);

        TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 2);

        h.confirm_trade_subscription(id1, "BTC/USD");
        h.confirm_trade_subscription(id2, "ETH/USD");

        TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 2);
        TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);
        TEST_CHECK(h.session.is_idle());
    }

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// Runner
// ----------------------------------------------------------------------------

#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_epoch_monotonicity();
    test_replay_once_per_epoch();
    test_no_duplicate_replay_across_epochs();
    test_reconnect_stress_convergence();

    std::cout << "\n[GROUP D - EPOCH & REPLAY STRESS TESTS PASSED]\n";
    return 0;
}
