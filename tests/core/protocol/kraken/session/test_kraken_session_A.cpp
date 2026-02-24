/*
===============================================================================
 protocol::kraken::Session - Group A Replay Tests
===============================================================================

Scope:
------
Validate core replay behavior after reconnect.

Covered:
A1 Single active subscription replayed
A2 Multiple channel replay (Trade + Book)
A3 No active subscriptions → no replay

These tests assume:
- MockWebSocket
- Deterministic poll-driven execution
- No real network I/O

===============================================================================
*/

#include <iostream>
#include <string>

#include "common/harness/session.hpp"


// ----------------------------------------------------------------------------
// A1 Single Active Subscription Replayed
// ----------------------------------------------------------------------------

void test_single_active_subscription_replayed() {
    std::cout << "[TEST] A1 Single active subscription replayed\n";

    test::SessionHarness h;
    h.connect();

    // Subscribe trade and receive ACK
    auto req_id = h.subscribe_trade("BTC/USD");
    h.confirm_trade_subscription(req_id, "BTC/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);

    h.force_reconnect();
    h.wait_for_epoch(2);

    // Replay should be pending
    TEST_CHECK(h.session.trade_subscriptions().has_pending_requests());

    // ACK replay
    h.confirm_trade_subscription(req_id, "BTC/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);
    TEST_CHECK(h.session.is_idle());

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// A2 Multiple Active Subscriptions Replayed (Trade + Book)
// ----------------------------------------------------------------------------

void test_multiple_channel_replay() {
    std::cout << "[TEST] A2 Multi-channel replay\n";

    test::SessionHarness h;
    h.connect();

    // Subscribe trade and receive ACK
    auto trade_req_id = h.subscribe_trade("BTC/USD");
    h.confirm_trade_subscription(trade_req_id, "BTC/USD");

    // Subscribe book and receive ACK
    int depth = 25;
    auto book_req_id = h.subscribe_book("ETH/USD", depth);
    h.confirm_book_subscription(book_req_id, "ETH/USD", depth);

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.book_subscriptions().active_symbols() == 1);

    // Reconnect
    h.force_reconnect();
    h.wait_for_epoch(2);

    // Both should be replayed
    TEST_CHECK(h.session.trade_subscriptions().has_pending_requests());
    TEST_CHECK(h.session.book_subscriptions().has_pending_requests());

    // ACK both replays
    h.confirm_trade_subscription(trade_req_id, "BTC/USD");
    h.confirm_book_subscription(book_req_id, "ETH/USD", depth);

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.book_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);
    TEST_CHECK(h.session.book_subscriptions().pending_requests() == 0);

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// A3 No Active Subscriptions → No Replay
// ----------------------------------------------------------------------------

void test_no_active_no_replay() {
    std::cout << "[TEST] A3 No active → no replay\n";

    test::SessionHarness h;
    h.connect();

    // Subscribe trade and receive ACK
    auto req_id = h.subscribe_trade("BTC/USD");
    h.confirm_trade_subscription(req_id, "BTC/USD");

    // Unsubscribe trade and receive ACK
    req_id = h.unsubscribe_trade("BTC/USD");
    h.confirm_trade_unsubscription(req_id, "BTC/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 0);

    // Reconnect
    h.force_reconnect();
    h.wait_for_epoch(2);

    // No replay expected
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// Runner
// ----------------------------------------------------------------------------

#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_single_active_subscription_replayed();
    test_multiple_channel_replay();
    test_no_active_no_replay();

    std::cout << "\n[GROUP A - SESSION REPLAY TESTS PASSED]\n";
    return 0;
}
