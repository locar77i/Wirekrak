/*
===============================================================================
 protocol::kraken::channel::Manager — Group B Unit Tests
===============================================================================

Scope:
------
These tests validate *unsubscribe-side state machine* behavior of
channel::Manager.

They focus exclusively on:
- Unsubscription request tracking
- ACK-driven removal semantics
- Rejection and no-op guarantees
- Active symbol preservation

These tests are:
- Fully deterministic
- Independent of subscribe tests
- Free of transport, replay, or parsing logic

Covered Requirements:
---------------------
B1. Unsubscribe happy path
B2. Unsubscribe rejected
B3. Unsubscribe non-active symbol

Non-Goals:
----------
- Subscribe behavior (covered in Group A)
- Replay sequencing
- Connection lifecycle
- Performance benchmarking

===============================================================================
*/

#include <iostream>
#include <vector>

#include "wirekrak/core/protocol/kraken/channel/manager.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::protocol;
using namespace wirekrak::core::protocol::kraken;


// -----------------------------------------------------------------------------
// Group B1: Unsubscribe happy path
// -----------------------------------------------------------------------------
void test_unsubscribe_happy_path() {
    std::cout << "[TEST] Group B1: unsubscribe happy path\n";

    channel::Manager mgr{Channel::Trade};

    const ctrl::req_id_t req_id{10};

    // Precondition: BTC/USD is active
    (void)mgr.register_subscription({"BTC/USD"}, req_id);
    mgr.process_subscribe_ack(req_id, "BTC/USD", true);

    TEST_CHECK(mgr.active_symbols() == 1);
    TEST_CHECK(mgr.has_active_symbols());

    const ctrl::req_id_t req_id2{11};

    // Unsubscribe BTC/USD
    (void)mgr.register_unsubscription({"BTC/USD"}, req_id2);
    mgr.process_unsubscribe_ack(req_id2, "BTC/USD", true);

    TEST_CHECK(mgr.active_symbols() == 0);
    TEST_CHECK(!mgr.has_active_symbols());
    TEST_CHECK(mgr.pending_requests() == 0);

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// Group B2: Unsubscribe rejected
// -----------------------------------------------------------------------------
void test_unsubscribe_rejected() {
    std::cout << "[TEST] Group B2: unsubscribe rejected\n";

    channel::Manager mgr{Channel::Trade};

    const ctrl::req_id_t req_id{10};

    // Precondition: BTC/USD is active
    (void)mgr.register_subscription({"BTC/USD"}, req_id);
    mgr.process_subscribe_ack(req_id, "BTC/USD", true);

    TEST_CHECK(mgr.active_symbols() == 1);

    const ctrl::req_id_t req_id2{11};

    // Attempt unsubscribe (rejected)
    (void)mgr.register_unsubscription({"BTC/USD"}, req_id2);
    mgr.process_unsubscribe_ack(req_id2, "BTC/USD", false);

    // Active state must remain unchanged
    TEST_CHECK(mgr.active_symbols() == 1);
    TEST_CHECK(mgr.has_active_symbols());
    TEST_CHECK(mgr.pending_requests() == 0);

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// Group B3: Unsubscribe non-active symbol
// -----------------------------------------------------------------------------
void test_unsubscribe_non_active_symbol() {
    std::cout << "[TEST] Group B3: unsubscribe non-active symbol\n";

    channel::Manager mgr{Channel::Trade};

    const ctrl::req_id_t req_id{10};

    // Precondition: BTC/USD is active
    (void)mgr.register_subscription({"BTC/USD"}, req_id);
    mgr.process_subscribe_ack(req_id, "BTC/USD", true);

    TEST_CHECK(mgr.active_symbols() == 1);

    const ctrl::req_id_t req_id2{12};
    
    // Attempt to unsubscribe ETH/USD (never active)
    (void)mgr.register_unsubscription({"ETH/USD"}, req_id2);
    mgr.process_unsubscribe_ack(req_id2, "ETH/USD", true);

    // Must be a safe no-op
    TEST_CHECK(mgr.active_symbols() == 1);
    TEST_CHECK(mgr.has_active_symbols());
    TEST_CHECK(mgr.pending_requests() == 0);

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------------
#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_unsubscribe_happy_path();
    test_unsubscribe_rejected();
    test_unsubscribe_non_active_symbol();

    std::cout << "\n[GROUP B — CHANNEL MANAGER UNSUBSCRIBE TESTS PASSED]\n";
    return 0;
}
