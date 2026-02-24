/*
===============================================================================
 protocol::kraken::Session - Group F Mixed Rejection + Reconnect Chaos
===============================================================================

Scope:
------
Stress replay DB correctness under chaotic but legal scenarios:

F1 Partial rejection before reconnect
F2 Reject after reconnect before replay ACK
F3 Mixed accept + reject across channels
F4 Reconnect storm with interleaved rejections

These validate:
- Replay DB mutates only by server truth
- Rejected symbols never replay
- Accepted symbols persist across reconnect
- Channel isolation under chaos
- Deterministic final convergence

===============================================================================
*/

#include <iostream>

#include "common/harness/session.hpp"


// ----------------------------------------------------------------------------
// F1 Partial rejection before reconnect
// ----------------------------------------------------------------------------

void test_partial_rejection_before_reconnect() {
    std::cout << "[TEST] F1 Partial rejection before reconnect\n";

    test::SessionHarness h;
    h.connect();

    auto id = h.subscribe_trade({"BTC/USD", "ETH/USD"});
    h.confirm_trade_subscription(id, "BTC/USD");
    h.reject_trade_subscription(id, "ETH/USD");

    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 1);

    h.force_reconnect();
    h.wait_for_epoch(2);

    // Only BTC/USD should replay
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 1);

    h.confirm_trade_subscription(id, "BTC/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);

    TEST_CHECK(!h.session.is_idle()); // because rejection exists

    h.drain_rejections(); // Drain rejection to reach protocol-idle state

    TEST_CHECK(h.session.is_idle());

    std::cout << "[TEST] OK\n";
}

// ----------------------------------------------------------------------------
// F2 Reject after reconnect before replay ACK
// ----------------------------------------------------------------------------

void test_reject_after_reconnect_before_ack() {
    std::cout << "[TEST] F2 Reject after reconnect before replay ACK\n";

    test::SessionHarness h;
    h.connect();

    auto id = h.subscribe_trade("BTC/USD");
    h.confirm_trade_subscription(id, "BTC/USD");

    h.force_reconnect();
    h.wait_for_epoch(2);

    // Replay pending
    TEST_CHECK(h.session.trade_subscriptions().has_pending_requests());

    // Server rejects before ACK
    h.reject_trade_subscription(id, "BTC/USD");

    h.drain();

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 0);
    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 0);
    TEST_CHECK(!h.session.is_idle()); // because rejection exists

    h.drain_rejections(); // Drain rejection to reach protocol-idle state

    TEST_CHECK(h.session.is_idle());

    std::cout << "[TEST] OK\n";
}

// ----------------------------------------------------------------------------
// F3 Mixed accept + reject across channels
// ----------------------------------------------------------------------------

void test_mixed_accept_reject_cross_channel() {
    std::cout << "[TEST] F3 Mixed accept + reject cross-channel\n";

    test::SessionHarness h;
    h.connect();

    auto t_id = h.subscribe_trade("BTC/USD");
    auto b_id = h.subscribe_book("ETH/USD", 25);

    h.confirm_trade_subscription(t_id, "BTC/USD");
    h.confirm_book_subscription(b_id, "ETH/USD", 25);

    // Reject trade only
    h.reject_trade_subscription(t_id, "BTC/USD");

    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 0);
    TEST_CHECK(h.session.replay_database().book_table().total_symbols() == 1);

    h.force_reconnect();
    h.wait_for_epoch(2);

    // Only book should replay
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);
    TEST_CHECK(h.session.book_subscriptions().pending_requests() == 1);

    h.confirm_book_subscription(b_id, "ETH/USD", 25);

    TEST_CHECK(h.session.book_subscriptions().active_symbols() == 1);
    TEST_CHECK(!h.session.is_idle()); // because rejection exists

    h.drain_rejections(); // Drain rejection to reach protocol-idle state

    TEST_CHECK(h.session.is_idle());

    std::cout << "[TEST] OK\n";
}

// ----------------------------------------------------------------------------
// F4 Reconnect storm with interleaved rejections
// ----------------------------------------------------------------------------

void test_reconnect_storm_with_rejections() {
    std::cout << "[TEST] F4 Reconnect storm with interleaved rejections\n";

    test::SessionHarness h;
    h.connect();

    auto id = h.subscribe_trade({"BTC/USD", "ETH/USD"});
    h.confirm_trade_subscription(id, "BTC/USD");
    h.confirm_trade_subscription(id, "ETH/USD");

    // Reconnect #1
    h.force_reconnect();
    h.wait_for_epoch(2);

    // Reject ETH during replay
    h.reject_trade_subscription(id, "ETH/USD");

    // ACK BTC replay
    h.confirm_trade_subscription(id, "BTC/USD");

    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 1);

    // Reconnect #2
    h.force_reconnect();
    h.wait_for_epoch(3);

    // Only BTC should replay
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 1);

    h.confirm_trade_subscription(id, "BTC/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
    TEST_CHECK(!h.session.is_idle()); // because rejection exists

    h.drain_rejections(); // Drain rejection to reach protocol-idle state

    TEST_CHECK(h.session.is_idle());

    std::cout << "[TEST] OK\n";
}

// ----------------------------------------------------------------------------
// Runner
// ----------------------------------------------------------------------------

#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_partial_rejection_before_reconnect();
    test_reject_after_reconnect_before_ack();
    test_mixed_accept_reject_cross_channel();
    test_reconnect_storm_with_rejections();

    std::cout << "\n[GROUP F - CHAOS REPLAY TESTS PASSED]\n";
    return 0;
}
