/*
===============================================================================
 protocol::kraken::channel::Manager — Group A Unit Tests
===============================================================================

Scope:
------
These tests validate the *pure protocol state machine* behavior of
channel::Manager.

They focus exclusively on:
- Pending subscription tracking
- ACK-driven state transitions
- Grouping by req_id
- Active symbol management

These tests are:
- Fully deterministic
- Free of transport, timing, or parsing logic
- Independent of Session, Connection, or WebSocket layers

Covered Requirements:
---------------------
A1. Subscribe happy path (single symbol)
A2. Subscribe rejected
A3. Multi-symbol subscribe with partial ACK

Non-Goals:
----------
- Transport or reconnection behavior
- Replay sequencing
- JSON parsing
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
// A1. Subscribe happy path (single symbol)
// -----------------------------------------------------------------------------
void test_subscribe_happy_path_single_symbol() {
    std::cout << "[TEST] Group A1: subscribe happy path (single symbol)\n";

    channel::Manager mgr{Channel::Trade};
    const ctrl::req_id_t req_id{10};

    (void)mgr.register_subscription({"BTC/USD"}, req_id);

    TEST_CHECK(mgr.has_pending_requests());
    TEST_CHECK(mgr.pending_subscription_requests() == 1);
    TEST_CHECK(mgr.pending_subscribe_symbols() == 1);
    TEST_CHECK(mgr.active_symbols() == 0);

    mgr.process_subscribe_ack(req_id, "BTC/USD", true);

    TEST_CHECK(!mgr.has_pending_requests());
    TEST_CHECK(mgr.pending_requests() == 0);
    TEST_CHECK(mgr.pending_symbols() == 0);
    TEST_CHECK(mgr.active_symbols() == 1);
    TEST_CHECK(mgr.has_active_symbols());

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// A2. Subscribe rejected
// -----------------------------------------------------------------------------
void test_subscribe_rejected() {
    std::cout << "[TEST] Group A2: subscribe rejected\n";

    channel::Manager mgr{Channel::Trade};
    const ctrl::req_id_t req_id{10};

    (void)mgr.register_subscription({"BTC/USD"}, req_id);

    TEST_CHECK(mgr.pending_subscription_requests() == 1);
    TEST_CHECK(mgr.pending_subscribe_symbols() == 1);

    mgr.process_subscribe_ack(req_id, "BTC/USD", false);

    TEST_CHECK(!mgr.has_pending_requests());
    TEST_CHECK(mgr.pending_requests() == 0);
    TEST_CHECK(mgr.pending_symbols() == 0);
    TEST_CHECK(mgr.active_symbols() == 0);

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// A3. Multi-symbol subscribe (partial ACK)
// -----------------------------------------------------------------------------
void test_multi_symbol_subscribe_partial_ack() {
    std::cout << "[TEST] Group A3: multi-symbol subscribe (partial ACK)\n";

    channel::Manager mgr{Channel::Trade};
    const ctrl::req_id_t req_id{10};

    (void)mgr.register_subscription({"BTC/USD", "ETH/USD"}, req_id);

    TEST_CHECK(mgr.pending_subscription_requests() == 1);
    TEST_CHECK(mgr.pending_subscribe_symbols() == 2);
    TEST_CHECK(mgr.active_symbols() == 0);

    // ACK only one symbol
    mgr.process_subscribe_ack(req_id, "BTC/USD", true);

    TEST_CHECK(mgr.has_pending_requests());
    TEST_CHECK(mgr.pending_subscription_requests() == 1); // same req_id still pending
    TEST_CHECK(mgr.pending_subscribe_symbols() == 1);
    TEST_CHECK(mgr.active_symbols() == 1);

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// A4. Multi-symbol subscribe (full ACK)
// -----------------------------------------------------------------------------
void test_multi_symbol_subscribe_full_ack() {
    std::cout << "[TEST] Group A4: multi-symbol subscribe (full ACK)\n";

    channel::Manager mgr{Channel::Trade};
    const ctrl::req_id_t req_id{10};

    (void)mgr.register_subscription({"BTC/USD", "ETH/USD"}, req_id);

    // First ACK (partial)
    mgr.process_subscribe_ack(req_id, "BTC/USD", true);

    TEST_CHECK(mgr.pending_subscription_requests() == 1);
    TEST_CHECK(mgr.pending_subscribe_symbols() == 1);
    TEST_CHECK(mgr.active_symbols() == 1);

    // Second ACK (completes req_id)
    mgr.process_subscribe_ack(req_id, "ETH/USD", true);

    TEST_CHECK(!mgr.has_pending_requests());
    TEST_CHECK(mgr.pending_requests() == 0);
    TEST_CHECK(mgr.pending_symbols() == 0);
    TEST_CHECK(mgr.active_symbols() == 2);
    TEST_CHECK(mgr.has_active_symbols());

    std::cout << "[TEST] OK\n";
}

// -----------------------------------------------------------------------------
// A5. Duplicate subscribe ACK is ignored
// -----------------------------------------------------------------------------
void test_duplicate_subscribe_ack_is_ignored() {
    std::cout << "[TEST] Group A5: duplicate subscribe ACK is ignored\n";

    channel::Manager mgr{Channel::Trade};
    const ctrl::req_id_t req_id{10};

    (void)mgr.register_subscription({"BTC/USD", "ETH/USD"}, req_id);

    mgr.process_subscribe_ack(req_id, "BTC/USD", true);
    mgr.process_subscribe_ack(req_id, "ETH/USD", true);

    // Sanity: fully completed
    TEST_CHECK(mgr.active_symbols() == 2);
    TEST_CHECK(!mgr.has_pending_requests());

    // Duplicate ACK (must be ignored safely)
    mgr.process_subscribe_ack(req_id, "BTC/USD", true);

    TEST_CHECK(mgr.active_symbols() == 2);
    TEST_CHECK(!mgr.has_pending_requests());
    TEST_CHECK(mgr.pending_requests() == 0);

    std::cout << "[TEST] OK\n";
}


// -----------------------------------------------------------------------------
// A6. Subscribe ACK with unknown req_id is ignored safely
// -----------------------------------------------------------------------------
void test_subscribe_ack_unknown_req_id_ignored() {
    std::cout << "[TEST] Group A6: subscribe ACK with unknown req_id is ignored\n";

    channel::Manager mgr{Channel::Trade};

    // No prior subscriptions registered

    // ACK arrives for unknown req_id
    mgr.process_subscribe_ack(42, "BTC/USD", true);

    // State must remain unchanged
    TEST_CHECK(!mgr.has_pending_requests());
    TEST_CHECK(mgr.pending_requests() == 0);
    TEST_CHECK(mgr.pending_symbols() == 0);
    TEST_CHECK(mgr.active_symbols() == 0);
    TEST_CHECK(!mgr.has_active_symbols());

    std::cout << "[TEST] OK\n";
}



#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Trace);

    test_subscribe_happy_path_single_symbol();
    test_subscribe_rejected();
    test_multi_symbol_subscribe_partial_ack();
    test_multi_symbol_subscribe_full_ack();
    test_duplicate_subscribe_ack_is_ignored();
    test_subscribe_ack_unknown_req_id_ignored();

    std::cout << "\n[GROUP A — CHANNEL MANAGER SUBSCRIBE TESTS PASSED]\n";
    return 0;
}
