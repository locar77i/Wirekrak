/*
===============================================================================
 protocol::kraken::Session - Group C Intent Convergence Tests
===============================================================================

Scope:
------
Validate Replay DB contract:

Replay DB stores user intent and is mutated only by server truth.

Contract:
1) If server rejects → intent removed
2) If server accepts → intent persists
3) If server silent → intent persists

These tests validate convergence behavior across reconnect cycles.

Covered:
C1 Initial subscribe rejected removes intent
C2 Replay rejected removes intent permanently
C3 Silent pending survives disconnect
C4 Unsubscribe accepted removes intent
C5 Unsubscribe rejected keeps intent

===============================================================================
*/

#include <iostream>
#include <string>

#include "common/harness/session.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::protocol::kraken::test;


// ----------------------------------------------------------------------------
// C1 Initial Subscribe Rejected Removes Intent
// ----------------------------------------------------------------------------

void test_initial_subscribe_rejected_removes_intent() {
    std::cout << "[TEST] C1 Initial subscribe rejected removes intent\n";

    SessionHarness h;
    h.connect();

    auto sub_id = h.subscribe_trade("BTC/USD");

    // Server rejects
    h.reject_trade_subscription(sub_id, "BTC/USD");

    TEST_CHECK(h.session.replay_database().trade_table().total_requests() == 0);
    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 0);

    // Reconnect → nothing to replay
    h.force_reconnect();
    h.wait_for_epoch(2);

    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// C2 Replay Rejected Removes Intent Permanently
// ----------------------------------------------------------------------------

void test_replay_rejected_removes_intent() {
    std::cout << "[TEST] C2 Replay rejected removes intent permanently\n";

    SessionHarness h;
    h.connect();

    auto sub_id = h.subscribe_trade("BTC/USD");
    h.confirm_trade_subscription(sub_id, "BTC/USD");

    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 1);

    // Reconnect → replay fires
    h.force_reconnect();
    h.wait_for_epoch(2);

    // Replay rejected
    h.reject_trade_subscription(sub_id, "BTC/USD");

    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 0);

    // Reconnect again → should NOT replay
    h.force_reconnect();
    h.wait_for_epoch(3);

    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// C3 Silent Pending Survives Disconnect
// ----------------------------------------------------------------------------

void test_silent_pending_survives_disconnect() {
    std::cout << "[TEST] C3 Silent pending survives disconnect\n";

    SessionHarness h;
    h.connect();

    auto sub_id = h.subscribe_trade("BTC/USD");

    // NO ACK

    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 1);

    // Reconnect
    h.force_reconnect();
    h.wait_for_epoch(2);

    // Replay should fire
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 1);

    // Now server accepts
    h.confirm_trade_subscription(sub_id, "BTC/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 1);

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// C4 Unsubscribe Accepted Removes Intent
// ----------------------------------------------------------------------------

void test_unsubscribe_accepted_removes_intent() {
    std::cout << "[TEST] C4 Unsubscribe accepted removes intent\n";

    SessionHarness h;
    h.connect();

    auto sub_id = h.subscribe_trade("BTC/USD");
    h.confirm_trade_subscription(sub_id, "BTC/USD");

    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 1);

    auto unsub_id = h.unsubscribe_trade("BTC/USD");
    h.confirm_trade_unsubscription(unsub_id, "BTC/USD");

    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 0);

    // Reconnect → no replay
    h.force_reconnect();
    h.wait_for_epoch(2);

    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// C5 Unsubscribe Rejected Keeps Intent
// ----------------------------------------------------------------------------

void test_unsubscribe_rejected_keeps_intent() {
    std::cout << "[TEST] C5 Unsubscribe rejected keeps intent\n";

    SessionHarness h;
    h.connect();

    auto sub_id = h.subscribe_trade("BTC/USD");
    h.confirm_trade_subscription(sub_id, "BTC/USD");

    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 1);

    auto unsub_id = h.unsubscribe_trade("BTC/USD");

    // Server rejects unsubscription
    h.reject_trade_unsubscription(unsub_id, "BTC/USD");

    // Intent must remain
    TEST_CHECK(h.session.replay_database().trade_table().total_symbols() == 1);

    // Reconnect → replay should happen
    h.force_reconnect();
    h.wait_for_epoch(2);

    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 1);

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// Runner
// ----------------------------------------------------------------------------

#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_initial_subscribe_rejected_removes_intent();
    test_replay_rejected_removes_intent();
    test_silent_pending_survives_disconnect();
    test_unsubscribe_accepted_removes_intent();
    test_unsubscribe_rejected_keeps_intent();

    std::cout << "\n[GROUP C - INTENT CONVERGENCE TESTS PASSED]\n";
    return 0;
}
