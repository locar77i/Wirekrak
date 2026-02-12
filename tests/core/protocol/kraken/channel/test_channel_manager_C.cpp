/*
===============================================================================
 protocol::kraken::channel::Manager — Group C Unit Tests
===============================================================================

Scope:
------
These tests validate *robustness and mixed-path behavior* of
channel::Manager that does not belong exclusively to subscribe
or unsubscribe flows.

They focus exclusively on:
- Rejection handling outside ACK paths
- Unknown req_id safety
- Full reset semantics

These tests are:
- Fully deterministic
- Pure state-machine tests
- Independent of transport, replay, or timing

Covered Requirements:
---------------------
C1. Rejection clears pending subscription
C2. Rejection with unknown req_id is ignored
C3. clear_all resets everything

Non-Goals:
----------
- ACK parsing correctness
- Transport-level behavior
- Session replay logic

===============================================================================
*/

#include <iostream>
#include <vector>

#include "wirekrak/core/protocol/kraken/channel/manager.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::protocol;
using namespace wirekrak::core::protocol::kraken;


// -----------------------------------------------------------------------------
// Group C1: Rejection clears pending subscription
// -----------------------------------------------------------------------------
void test_rejection_clears_pending_subscription() {
    std::cout << "[TEST] Group C1: rejection clears pending subscription\n";

    channel::Manager mgr{Channel::Trade};

    // Create true pending subscription
    mgr.register_subscription({"BTC/USD"}, 1);

    TEST_CHECK(mgr.has_pending_requests());
    TEST_CHECK(mgr.total_symbols() == 1);

    mgr.try_process_rejection(1, "BTC/USD");

    TEST_CHECK(!mgr.has_pending_requests());
    TEST_CHECK(mgr.total_symbols() == 0);
    TEST_CHECK(mgr.active_symbols() == 0);

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// Group C2: Rejection with unknown req_id is ignored
// -----------------------------------------------------------------------------
void test_rejection_unknown_req_id_is_ignored() {
    std::cout << "[TEST] Group C2: rejection with unknown req_id is ignored\n";

    channel::Manager mgr{Channel::Trade};

    // Precondition: BTC/USD is active
    mgr.register_subscription({"BTC/USD"}, 1);
    mgr.process_subscribe_ack(1, "BTC/USD", true);

    TEST_CHECK(mgr.active_symbols() == 1);
    TEST_CHECK(!mgr.has_pending_requests());

    // Unknown req_id rejection
    mgr.try_process_rejection(999, "BTC/USD");

    // No state change allowed
    TEST_CHECK(mgr.active_symbols() == 1);
    TEST_CHECK(!mgr.has_pending_requests());
    TEST_CHECK(mgr.pending_requests() == 0);

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// Group C3: clear_all resets everything
// -----------------------------------------------------------------------------
void test_clear_all_resets_everything() {
    std::cout << "[TEST] Group C3: clear_all resets everything\n";

    channel::Manager mgr{Channel::Trade};

    // Create mixed state
    mgr.register_subscription({"BTC/USD", "ETH/USD"}, 1);
    mgr.process_subscribe_ack(1, "BTC/USD", true); // partial

    TEST_CHECK(mgr.active_symbols() == 1);
    TEST_CHECK(mgr.has_pending_requests());

    // Full reset
    mgr.clear_all();

    TEST_CHECK(!mgr.has_pending_requests());
    TEST_CHECK(mgr.pending_requests() == 0);
    TEST_CHECK(mgr.pending_symbols() == 0);
    TEST_CHECK(mgr.active_symbols() == 0);
    TEST_CHECK(!mgr.has_active_symbols());

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_rejection_clears_pending_subscription();
    test_rejection_unknown_req_id_is_ignored();
    test_clear_all_resets_everything();

    std::cout << "\n[GROUP C — CHANNEL MANAGER ROBUSTNESS TESTS PASSED]\n";
    return 0;
}
