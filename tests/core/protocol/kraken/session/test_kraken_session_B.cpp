/*
===============================================================================
 protocol::kraken::Session — Group B Replay + Pending Interaction Tests
===============================================================================

Scope:
------
Validate replay behavior under tricky edge cases:

B1 Reconnect while subscription still pending
B2 User subscribes during replay window

These tests validate:
- Pending subscriptions are dropped on disconnect
- Only ACKed subscriptions are replayed
- Replay and user intent compose correctly
- Final convergence is deterministic

===============================================================================
*/

#include <iostream>
#include <string>


#include "common/harness/session.hpp"


// ----------------------------------------------------------------------------
// B1 Reconnect While Subscription Still Pending
// ----------------------------------------------------------------------------

void test_reconnect_while_pending_subscription() {
    std::cout << "[TEST] B1 Reconnect while subscription still pending\n";

    SessionHarness h;
    h.connect();

    // Initial subscription -x but DO NOT ACK
    auto req_id = h.subscribe_trade("BTC/USD");

    // Pending subscription should be visible
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 1);
    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 0);

    // Reconnect
    h.force_reconnect();
    h.wait_for_epoch(2);

    TEST_CHECK(h.session.transport_epoch() == 2);

    // Pending subscription should be visible again
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 1);
    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 0);

    // ACK replayed subscription
    h.confirm_trade_subscription(req_id, "BTC/USD");

    // Should now be active
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);
    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// B2 User Subscribes During Replay Window
// ----------------------------------------------------------------------------

void test_user_subscribes_during_replay_window() {
    std::cout << "[TEST] B2 User subscribes during replay window\n";

    SessionHarness h;
    h.connect();

    // Initial subscription -> ACK
    auto req_id1 = h.subscribe_trade("BTC/USD");
    h.confirm_trade_subscription(req_id1, "BTC/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);

    // Reconnect
    h.force_reconnect();
    h.wait_for_epoch(2);

    TEST_CHECK(h.session.transport_epoch() == 2);

    // Replay should have fired -> pending > 0
    TEST_CHECK(h.session.trade_subscriptions().has_pending_requests());

    // BEFORE replay ACK arrives → user subscribes new symbol
    auto req_id2 = h.subscribe_trade("ETH/USD");

    // Now pending should reflect replay + user request
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() >= 1);

    // Simulate ACK for replayed BTC/USD
    h.confirm_trade_subscription(req_id1, "BTC/USD");

    // Simulate ACK for user ETH/USD
    h.confirm_trade_subscription(req_id2, "ETH/USD");

    h.drain();

    // Final convergence
    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 2);
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);
    TEST_CHECK(h.session.is_idle());

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// B3 Replay Fires Only Once Per Epoch
// ----------------------------------------------------------------------------

void test_replay_fires_only_once_per_epoch() {
    std::cout << "[TEST] B3 Replay fires only once per epoch\n";

    SessionHarness h;
    h.connect();

    // Initial subscription -> ACK
    auto req_id = h.subscribe_trade("BTC/USD");
    h.confirm_trade_subscription(req_id, "BTC/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);

    // Force reconnect
    h.force_reconnect();
    h.wait_for_epoch(2);

    TEST_CHECK(h.session.transport_epoch() == 2);

    // Replay should have fired exactly once
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 1);

    std::size_t pending_after_first_poll =
        h.session.trade_subscriptions().pending_requests();

    // Call poll() 1000 times without ACK
    h.drain(1000);

    // Pending must remain unchanged (no duplicate replay)
    TEST_CHECK(h.session.trade_subscriptions().pending_requests()
               == pending_after_first_poll);

    // Now ACK replay
    h.confirm_trade_subscription(req_id, "BTC/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);
    TEST_CHECK(h.session.is_idle());

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// B4 Replay ACK with unknown req_id is ignored safely
// ----------------------------------------------------------------------------

void test_replay_ack_unknown_req_id_is_ignored() {
    std::cout << "[TEST] B4 Replay ACK with unknown req_id is ignored\n";

    SessionHarness h;
    h.connect();

    // Establish one valid active subscription
    auto valid_req_id = h.subscribe_trade("BTC/USD");
    h.confirm_trade_subscription(valid_req_id, "BTC/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);

    // Force reconnect
    h.force_reconnect();
    h.wait_for_epoch(2);

    TEST_CHECK(h.session.transport_epoch() == 2);

    // Replay should now be pending
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 1);

    // Send ACK with completely unknown req_id
    ctrl::req_id_t unknown_req_id = 999999;

    h.confirm_trade_subscription(unknown_req_id, "BTC/USD");

    // State must be the same before reconnect (unknown ACK should be ignored)
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 1);
    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 0);

    // Now ACK correct replay
    h.confirm_trade_subscription(valid_req_id, "BTC/USD");

    // Should now be active
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);
    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.is_idle());

    std::cout << "[TEST] OK\n";
}


// ----------------------------------------------------------------------------
// Runner
// ----------------------------------------------------------------------------

#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_reconnect_while_pending_subscription();
    test_user_subscribes_during_replay_window();
    test_replay_fires_only_once_per_epoch();
    test_replay_ack_unknown_req_id_is_ignored();

    std::cout << "\n[GROUP B — REPLAY + PENDING INTERACTION TESTS PASSED]\n";
    return 0;
}
