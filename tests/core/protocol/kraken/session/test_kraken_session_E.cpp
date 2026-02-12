/*
===============================================================================
 protocol::kraken::Session - Group E Cross-Channel Replay Isolation Tests
===============================================================================

Scope:
------
Validate that replay and rejection logic is fully isolated per channel.

Covered:
E1 Trade replay does not affect Book
E2 Book replay does not affect Trade
E3 Rejection isolation across channels
E4 Replay DB table independence
E5 Multi-channel reconnect stress

===============================================================================
*/

#include <iostream>
#include <string>

#include "common/harness/session.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::protocol::kraken::test;


// ----------------------------------------------------------------------------
// E1 Trade replay does not affect Book
// ----------------------------------------------------------------------------

void test_trade_replay_isolated_from_book() {
    std::cout << "[TEST] E1 Trade replay isolation\n";

    SessionHarness h;
    h.connect();

    auto trade_id = h.subscribe_trade("BTC/USD");
    h.confirm_trade_subscription(trade_id, "BTC/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.book_subscriptions().active_symbols() == 0);

    h.force_reconnect();
    h.wait_for_epoch(2);

    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 1);
    TEST_CHECK(h.session.book_subscriptions().pending_requests() == 0);

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// E2 Book replay does not affect Trade
// ----------------------------------------------------------------------------

void test_book_replay_isolated_from_trade() {
    std::cout << "[TEST] E2 Book replay isolation\n";

    SessionHarness h;
    h.connect();

    auto book_id = h.subscribe_book("ETH/USD", 25);
    h.confirm_book_subscription(book_id, "ETH/USD", 25);

    TEST_CHECK(h.session.book_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 0);

    h.force_reconnect();
    h.wait_for_epoch(2);

    TEST_CHECK(h.session.book_subscriptions().pending_requests() == 1);
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// E3.1 Rejection isolation across channels
// ----------------------------------------------------------------------------

void test_rejection_isolated_per_channel() {
    std::cout << "[TEST] E3.1 Rejection isolation\n";

    SessionHarness h;
    h.connect();

    auto trade_id = h.subscribe_trade("BTC/USD");
    auto book_id  = h.subscribe_book("ETH/USD", 25);

    // Confirm only book
    h.confirm_book_subscription(book_id, "ETH/USD", 25);

    // Reject trade BEFORE ACK
    h.reject_trade_subscription(trade_id, "BTC/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 0);
    TEST_CHECK(h.session.book_subscriptions().active_symbols() == 1);

    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 0);
    TEST_CHECK(h.session.replay_database().book_table().total_symbols() == 1);

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// E3.2 Rejection isolation across reconnect
// ----------------------------------------------------------------------------

void test_rejection_isolation_with_reconnect() {
    std::cout << "[TEST] E3.2 Rejection isolation + reconnect\n";

    SessionHarness h;
    h.connect();

    // Subscribe trade + book
    auto trade_id = h.subscribe_trade("BTC/USD");
    auto book_id  = h.subscribe_book("ETH/USD", 25);

    // ACK both
    h.confirm_trade_subscription(trade_id, "BTC/USD");
    h.confirm_book_subscription(book_id, "ETH/USD", 25);

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.book_subscriptions().active_symbols() == 1);

    // ---------------------------------------------------------------------
    // Now simulate server rejection for TRADE only
    // (valid scenario: rejection notice received after subscription)
    // ---------------------------------------------------------------------

    h.reject_trade_subscription(trade_id, "BTC/USD");

    // Trade intent must be removed from Replay DB
    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 0);

    // Book intent must remain untouched
    TEST_CHECK(h.session.replay_database().book_table().total_symbols() == 1);

    // ---------------------------------------------------------------------
    // Force reconnect
    // ---------------------------------------------------------------------

    h.force_reconnect();
    h.wait_for_epoch(2);

    TEST_CHECK(h.session.transport_epoch() == 2);

    // Managers were reset on disconnect → active should now be 0
    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 0);
    TEST_CHECK(h.session.book_subscriptions().active_symbols() == 0);

    // ---------------------------------------------------------------------
    // Replay should fire ONLY for Book
    // ---------------------------------------------------------------------

    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);
    TEST_CHECK(h.session.book_subscriptions().pending_requests() == 1);

    // ACK replayed Book subscription
    h.confirm_book_subscription(book_id, "ETH/USD", 25);

    TEST_CHECK(h.session.book_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 0);

    TEST_CHECK(!h.session.is_idle()); // because rejection exists

    // Drain rejection to reach protocol-idle state
    h.drain_rejections();

    TEST_CHECK(h.session.is_idle());

    std::cout << "[TEST] OK\n";
}



// ----------------------------------------------------------------------------
// E4 Replay DB table independence
// ----------------------------------------------------------------------------

void test_replay_database_isolated_tables() {
    std::cout << "[TEST] E4 Replay DB isolation\n";

    SessionHarness h;
    h.connect();

    auto trade_id = h.subscribe_trade("BTC/USD");
    auto book_id  = h.subscribe_book("ETH/USD", 25);

    h.confirm_trade_subscription(trade_id, "BTC/USD");
    h.confirm_book_subscription(book_id, "ETH/USD", 25);

    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 1);
    TEST_CHECK(h.session.replay_database().book_table().total_symbols() == 1);

    h.force_reconnect();
    h.wait_for_epoch(2);

    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 1);
    TEST_CHECK(h.session.replay_database().book_table().total_symbols() == 1);

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// E5 Multi-channel reconnect stress
// ----------------------------------------------------------------------------

void test_multi_channel_reconnect_stress() {
    std::cout << "[TEST] E5 Multi-channel reconnect stress\n";

    SessionHarness h;
    h.connect();

    auto trade_id = h.subscribe_trade("BTC/USD");
    auto book_id  = h.subscribe_book("ETH/USD", 25);

    h.confirm_trade_subscription(trade_id, "BTC/USD");
    h.confirm_book_subscription(book_id, "ETH/USD", 25);

    for (int i = 2; i <= 5; ++i) {
        h.force_reconnect();
        h.wait_for_epoch(i);

        TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 1);
        TEST_CHECK(h.session.book_subscriptions().pending_requests() == 1);

        h.confirm_trade_subscription(trade_id, "BTC/USD");
        h.confirm_book_subscription(book_id, "ETH/USD", 25);

        TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
        TEST_CHECK(h.session.book_subscriptions().active_symbols() == 1);
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

    test_trade_replay_isolated_from_book();
    test_book_replay_isolated_from_trade();
    test_rejection_isolated_per_channel();
    test_rejection_isolation_with_reconnect();
    test_replay_database_isolated_tables();
    test_multi_channel_reconnect_stress();

    std::cout << "\n[GROUP E - CROSS-CHANNEL REPLAY ISOLATION PASSED]\n";
    return 0;
}
