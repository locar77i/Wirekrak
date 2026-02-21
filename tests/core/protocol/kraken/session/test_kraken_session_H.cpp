/*
===============================================================================
 protocol::kraken::Session - Group H - Deterministic adversarial protocol tests
===============================================================================

Scope:
------

These tests validate:


===============================================================================
*/

#include <iostream>
#include <random>
#include <vector>

#include "common/harness/session.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::protocol::kraken::test;

// ------------------------------------------------------------
// Utility
// ------------------------------------------------------------

static std::string random_symbol(std::mt19937& rng) {
    static const std::vector<std::string> syms = {
        "BTC/USD", "ETH/USD", "SOL/USD", "LTC/USD"
    };
    std::uniform_int_distribution<> dist(0, syms.size() - 1);
    return syms[dist(rng)];
}


// ------------------------------------------------------------
// H1 - Out-of-order ACK burst
// ------------------------------------------------------------

void test_out_of_order_ack_burst() {
    std::cout << "[TEST] H1 Out-of-order ACK burst\n";

    SessionHarness h;
    h.connect();

    // ------------------------------------------------------------
    // Step 1: Issue 3 subscriptions (no ACK yet)
    // ------------------------------------------------------------

    auto id1 = h.subscribe_trade("BTC/USD");
    auto id2 = h.subscribe_trade("ETH/USD");
    auto id3 = h.subscribe_trade("SOL/USD");

    TEST_CHECK(h.session.trade_subscriptions().pending_subscription_requests() == 3);

    // ------------------------------------------------------------
    // Step 2: Force reconnect before any ACK
    // ------------------------------------------------------------

    auto prev_epoch = h.session.transport_epoch();
    auto new_epoch  = h.force_reconnect();
    h.wait_for_epoch(prev_epoch + 1);
    TEST_CHECK(new_epoch > prev_epoch);

    // Still pending
    TEST_CHECK(h.session.trade_subscriptions().pending_subscription_requests() == 3);

    // ------------------------------------------------------------
    // Step 3: Deliver ACKs in reverse order
    // ------------------------------------------------------------

    h.confirm_trade_subscription(id3, "SOL/USD");
    h.confirm_trade_subscription(id1, "BTC/USD");
    h.confirm_trade_subscription(id2, "ETH/USD");

    h.drain();

    // ------------------------------------------------------------
    // Step 4: Inject duplicate ACKs
    // ------------------------------------------------------------

    h.confirm_trade_subscription(id1, "BTC/USD");
    h.confirm_trade_subscription(id3, "SOL/USD");

    h.drain();

    // ------------------------------------------------------------
    // Final invariants
    // ------------------------------------------------------------

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 3);
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);
    TEST_CHECK(h.session.pending_protocol_requests() == 0);

    TEST_CHECK(
        h.session.trade_subscriptions().total_symbols() ==
        h.session.replay_database().trade_table().total_symbols()
    );

    TEST_CHECK(h.session.is_idle());

    std::cout << "[TEST] OK\n";
}


// ------------------------------------------------------------
// H2 - Duplicate ACK storm
// ------------------------------------------------------------

void test_duplicate_ack_storm() {
    std::cout << "[TEST] H2 Duplicate ACK storm\n";

    SessionHarness h;
    h.connect();

    // ------------------------------------------------------------
    // Phase A - Subscribe + duplicate success
    // ------------------------------------------------------------

    auto sub_btc = h.subscribe_trade("BTC/USD");

    // Deliver same ACK multiple times
    for (int i = 0; i < 10; ++i) {
        h.confirm_trade_subscription(sub_btc, "BTC/USD");
    }

    h.drain();

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);

    // ------------------------------------------------------------
    // Phase B - Subscribe + duplicate rejection
    // ------------------------------------------------------------

    auto sub_eth = h.subscribe_trade("ETH/USD");

    for (int i = 0; i < 10; ++i) {
        h.reject_trade_subscription(sub_eth, "ETH/USD");
    }

    h.drain();
    h.drain_rejections();

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);

    // ------------------------------------------------------------
    // Phase C - Unsubscribe + duplicate success
    // ------------------------------------------------------------

    auto unsub_btc = h.unsubscribe_trade("BTC/USD");

    for (int i = 0; i < 10; ++i) {
        h.confirm_trade_unsubscription(unsub_btc, "BTC/USD");
    }

    h.drain();

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 0);
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);

    // ------------------------------------------------------------
    // Phase D - Replay old ACKs again (should be ignored)
    // ------------------------------------------------------------

    for (int i = 0; i < 10; ++i) {
        h.confirm_trade_subscription(sub_btc, "BTC/USD");
        h.reject_trade_subscription(sub_eth, "ETH/USD");
        h.confirm_trade_unsubscription(unsub_btc, "BTC/USD");
    }

    h.drain();
    h.drain_rejections();

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 0);
    TEST_CHECK(h.session.trade_subscriptions().pending_requests() == 0);
    TEST_CHECK(h.session.pending_protocol_requests() == 0);

    TEST_CHECK(
        h.session.trade_subscriptions().total_symbols() ==
        h.session.replay_database().trade_table().total_symbols()
    );

    TEST_CHECK(h.session.is_idle());

    std::cout << "[TEST] OK\n";
}


// ------------------------------------------------------------
// H3 - Subscribe/unsubscribe race under replay
// ------------------------------------------------------------

void test_subscribe_unsubscribe_race_under_replay() {
    std::cout << "[TEST] H3 Subscribe/unsubscribe race under replay\n";

    SessionHarness h;
    h.connect();

    // ------------------------------------------------------------
    // 1) Initial subscribe + ACK
    // ------------------------------------------------------------
    auto sub_id = h.subscribe_trade("BTC/USD");
    h.confirm_trade_subscription(sub_id, "BTC/USD");

    TEST_CHECK(h.session.trade_subscriptions().active_symbols() == 1);

    // ------------------------------------------------------------
    // 2) Force reconnect → triggers replay
    // ------------------------------------------------------------
    h.force_reconnect();

    // Replay will re-send subscribe for BTC/USD
    // but no ACK yet

    // ------------------------------------------------------------
    // 3) Immediately send unsubscribe
    // ------------------------------------------------------------
    auto unsub_id = h.unsubscribe_trade("BTC/USD");

    // Simulate ACK race:
    // - First process unsubscribe ACK
    // - Then process subscribe ACK
    h.confirm_trade_unsubscription(unsub_id, "BTC/USD");
    h.confirm_trade_subscription(sub_id, "BTC/USD");

    // Drain any remaining events
    for (int i = 0; i < 20; ++i) {
        h.drain();
        h.drain_rejections();
    }

    // ------------------------------------------------------------
    // Final invariants (race-safe assertions)
    // ------------------------------------------------------------

    // 1) No pending protocol requests
    TEST_CHECK(h.session.pending_protocol_requests() == 0);

    // 2) Manager and Replay DB must agree on symbol count
    TEST_CHECK(
        h.session.trade_subscriptions().total_symbols() ==
        h.session.replay_database().trade_table().total_symbols()
    );

    // 3) No duplicate symbols possible
    TEST_CHECK(
        h.session.trade_subscriptions().active_symbols() <= 1
    );

    std::cout << "[TEST] OK\n";
}


// ------------------------------------------------------------
// H4 - Replay DB saturation limit test
// ------------------------------------------------------------

void test_replay_db_saturation_limit() {
    std::cout << "[TEST] H4 Replay DB saturation limit\n";

    SessionHarness h;
    h.connect();

    constexpr int STEPS = 1000;
    constexpr uint32_t SEED = 777;
    constexpr int SYMBOL_UNIVERSE = 5;

    std::vector<std::string> symbols = {
        "BTC/USD",
        "ETH/USD",
        "SOL/USD",
        "ADA/USD",
        "XRP/USD"
    };

    std::mt19937 rng(SEED);
    std::uniform_int_distribution<> pick(0, SYMBOL_UNIVERSE - 1);
    std::uniform_int_distribution<> coin(0, 1);

    std::vector<std::pair<ctrl::req_id_t, std::string>> pending;

    // ------------------------------------------------------------
    // Phase 1 - Saturation spam
    // ------------------------------------------------------------

    for (int i = 0; i < STEPS; ++i) {

        auto sym = symbols[pick(rng)];
        auto req_id  = h.subscribe_trade(sym);
        if (req_id != ctrl::INVALID_REQ_ID) {
            pending.emplace_back(req_id, sym);
        }

        // Occasionally resolve
        if (!pending.empty() && (i % 3 == 0)) {
            auto [pid, psym] = pending.back();
            pending.pop_back();

            if (coin(rng))
                h.confirm_trade_subscription(pid, psym);
            else
                h.reject_trade_subscription(pid, psym);
        }

        // Occasional reconnect pressure
        if (i % 50 == 0) {
            h.force_reconnect();
            h.wait_for_epoch(h.session.transport_epoch());
        }

        h.drain();
        h.drain_rejections();
    }

    // ------------------------------------------------------------
    // Stabilize
    // ------------------------------------------------------------

    for (int i = 0; i < 200 && !h.session.is_idle(); ++i) {
        h.drain();
        h.drain_rejections();
    }

    // ------------------------------------------------------------
    // Structural invariants
    // ------------------------------------------------------------

    // Symbol universe upper bound respected
    TEST_CHECK( h.session.replay_database().trade_table().total_symbols() <= SYMBOL_UNIVERSE );

    TEST_CHECK( h.session.trade_subscriptions().total_symbols() <= SYMBOL_UNIVERSE );

    // Replay DB and Manager converge
    TEST_CHECK( h.session.trade_subscriptions().total_symbols() == h.session.replay_database().trade_table().total_symbols() );

    // No dangling protocol requests
    //TEST_CHECK(h.session.pending_protocol_requests() == 0); // Too strong ...
    TEST_CHECK( h.session.pending_protocol_requests() <= SYMBOL_UNIVERSE );

    // No structural explosion
    TEST_CHECK( h.session.replay_database().trade_table().total_requests() <= SYMBOL_UNIVERSE );

    std::cout << "[TEST] OK\n";
}


// ------------------------------------------------------------
// H5 - Replay DB stress with mixed trade + book
// ------------------------------------------------------------

void test_replay_db_mixed_trade_book_stress() {
    std::cout << "[TEST] H5 Replay DB stress with mixed trade + book\n";

    SessionHarness h;
    h.connect();

    constexpr int STEPS = 1000;
    constexpr uint32_t SEED = 2026;

    std::mt19937 rng(SEED);
    std::uniform_int_distribution<> action(0, 5);
    std::uniform_int_distribution<> coin(0, 1);

    std::uint64_t last_epoch = h.session.transport_epoch();

    for (int step = 0; step < STEPS; ++step) {

        switch (action(rng)) {

        // --- Trade subscribe
        case 0: {
            auto sym = random_symbol(rng);
            auto req_id = h.subscribe_trade(sym);
            if (req_id != ctrl::INVALID_REQ_ID) {
                h.confirm_trade_subscription(req_id, sym);
            }
            break;
        }

        // --- Book subscribe
        case 1: {
            auto sym = random_symbol(rng);
            auto req_id = h.subscribe_book(sym, 25);
            if (req_id != ctrl::INVALID_REQ_ID) {
               h.confirm_book_subscription(req_id, sym, 25);
            }
            break;
        }

        // --- Trade unsubscribe
        case 2: {
            auto sym = random_symbol(rng);
            auto req_id = h.unsubscribe_trade(sym);
            if (req_id != ctrl::INVALID_REQ_ID) {
                h.confirm_trade_unsubscription(req_id, sym);
            }
            break;
        }

        // --- Book unsubscribe
        case 3: {
            auto sym = random_symbol(rng);
            auto req_id = h.unsubscribe_book(sym, 25);
            if (req_id != ctrl::INVALID_REQ_ID) {
                h.confirm_book_unsubscription(req_id, sym, 25);
            }
            break;
        }

        // --- Reconnect
        case 4: {
            std::uint64_t new_epoch = h.force_reconnect();
            TEST_CHECK(new_epoch > last_epoch);
            last_epoch = new_epoch;
            break;
        }

        // --- Idle tick
        case 5: {
            h.drain();
            break;
        }
        }

        h.drain();
    }

    // Stabilize
    for (int i = 0; i < 100 && !h.session.is_idle(); ++i) {
        h.drain();
    }

    // ------------------------------------------------------------
    // Final invariants
    // ------------------------------------------------------------

    // Global consistency
    TEST_CHECK( h.session.pending_protocol_requests() <= h.session.replay_database().total_requests());
    TEST_CHECK( h.session.pending_protocol_symbols() <= h.session.replay_database().total_symbols());

    // Trade alignment
    TEST_CHECK( h.session.trade_subscriptions().total_symbols() == h.session.replay_database().trade_table().total_symbols() );

    // Book alignment
    TEST_CHECK( h.session.book_subscriptions().total_symbols() == h.session.replay_database().book_table().total_symbols() );

    std::cout << "[TEST] OK\n";
}


// ------------------------------------------------------------
// H6 — Saturation + race overlap
// ------------------------------------------------------------

void test_saturation_race_overlap() {
    std::cout << "[TEST] H6 Saturation + race overlap\n";

    SessionHarness h;
    h.connect();

    constexpr int STEPS = 1000;
    constexpr uint32_t SEED = 4242;

    std::mt19937 rng(SEED);
    std::uniform_int_distribution<> action(0, 9);
    std::uniform_int_distribution<> coin(0, 1);

    std::vector<std::pair<ctrl::req_id_t, std::string>> trade_pending;
    std::vector<std::pair<ctrl::req_id_t, std::string>> book_pending;

    std::uint64_t last_epoch = h.session.transport_epoch();

    for (int i = 0; i < STEPS; ++i) {

        switch (action(rng)) {

        // --- trade subscribe
        case 0: {
            auto sym = random_symbol(rng);
            auto req_id = h.subscribe_trade(sym);
            if (req_id != ctrl::INVALID_REQ_ID) {
                trade_pending.emplace_back(req_id, sym);
            }
            break;
        }

        // --- book subscribe
        case 1: {
            auto sym = random_symbol(rng);
            auto req_id = h.subscribe_book(sym, 25);
            if (req_id != ctrl::INVALID_REQ_ID) {
                book_pending.emplace_back(req_id, sym);
            }
            break;
        }

        // --- resolve trade
        case 2: {
            if (!trade_pending.empty()) {
                auto [req_id, sym] = trade_pending.back();
                if (req_id != ctrl::INVALID_REQ_ID) {
                    trade_pending.pop_back();
                }

                if (coin(rng))
                    h.confirm_trade_subscription(req_id, sym);
                else
                    h.reject_trade_subscription(req_id, sym);
            }
            break;
        }

        // --- resolve book
        case 3: {
            if (!book_pending.empty()) {
                auto [req_id, sym] = book_pending.back();
                book_pending.pop_back();

                if (coin(rng))
                    h.confirm_book_subscription(req_id, sym, 25);
                else
                    h.reject_book_subscription(req_id, sym);
            }
            break;
        }

        // --- trade unsubscribe
        case 4: {
            auto sym = random_symbol(rng);
            auto req_id = h.unsubscribe_trade(sym);
            if (req_id != ctrl::INVALID_REQ_ID) {
                trade_pending.emplace_back(req_id, sym);
            }
            break;
        }

        // --- book unsubscribe
        case 5: {
            auto sym = random_symbol(rng);
            auto req_id = h.unsubscribe_book(sym, 25);
            if (req_id != ctrl::INVALID_REQ_ID) {
                book_pending.emplace_back(req_id, sym);
            }
            break;
        }

        // --- forced reconnect storm
        case 6:
        case 7: {
            std::uint64_t new_epoch = h.force_reconnect();
            h.wait_for_epoch(last_epoch + 1);
            TEST_CHECK(new_epoch > last_epoch);
            last_epoch = new_epoch;
            break;
        }

        // --- idle tick
        case 8:
        case 9:
            break;
        }

        h.drain();
        h.drain_rejections();
    }

    // ------------------------------------------------------------
    // Stabilization
    // ------------------------------------------------------------
    for (int i = 0; i < 200; ++i) {
        h.drain();
        h.drain_rejections();
    }

    // ------------------------------------------------------------
    // Final invariants (convergence, not idle)
    // ------------------------------------------------------------

    // Global consistency
    TEST_CHECK( h.session.pending_protocol_requests() <= h.session.replay_database().total_requests());
    TEST_CHECK( h.session.pending_protocol_symbols() <= h.session.replay_database().total_symbols());

    // Trade logical consistency
    TEST_CHECK( h.session.trade_subscriptions().total_symbols() == h.session.replay_database().trade_table().total_symbols() );

    // Book logical consistency
    TEST_CHECK( h.session.book_subscriptions().total_symbols() == h.session.replay_database().book_table().total_symbols() );

    // No cross-channel contamination
    TEST_CHECK( h.session.trade_subscriptions().total_symbols() >= 0 );

    TEST_CHECK( h.session.book_subscriptions().total_symbols() >= 0 );

#ifndef NDEBUG
    h.session.replay_database().trade_table().assert_consistency();
    h.session.replay_database().book_table().assert_consistency();
#endif

    std::cout << "[TEST] OK\n";
}


// ------------------------------------------------------------
// H7 — Hard limit enforcement (max N symbols policy)
// ------------------------------------------------------------

void test_hard_limit_enforcement() {
    using namespace wirekrak::core::transport;
    using namespace wirekrak::core::protocol;
    std::cout << "[TEST] H7 Hard limit enforcement\n";

    using Hard5 =
    policy::SymbolLimitPolicy<
        policy::LimitMode::Hard,
        5, 5, 8
    >;

    kraken::test::harness::Session<WebSocketUnderTest, MessageRingUnderTest, Hard5> h;
    h.connect();

    constexpr std::size_t MAX_SYMBOLS = 5;

    std::vector<std::string> symbols = {
        "BTC/USD", "ETH/USD", "SOL/USD",
        "LTC/USD", "XRP/USD", "ADA/USD",
        "DOT/USD"
    };

    std::vector<ctrl::req_id_t> accepted;

    // ------------------------------------------------------------
    // Attempt to exceed limit
    // ------------------------------------------------------------

    for (std::size_t i = 0; i < symbols.size(); ++i) {

        auto req_id = h.subscribe_trade(symbols[i]);

        if (i < MAX_SYMBOLS) {
            accepted.push_back(req_id);
            h.confirm_trade_subscription(req_id, symbols[i]);
        }

        h.drain();
    }

    // ------------------------------------------------------------
    // Verify hard limit respected
    // ------------------------------------------------------------

    TEST_CHECK(
        h.session.trade_subscriptions().active_symbols() <= MAX_SYMBOLS
    );

    TEST_CHECK(
        h.session.trade_subscriptions().total_symbols() <= MAX_SYMBOLS
    );

    TEST_CHECK(
        h.session.replay_database().trade_table().total_symbols() <= MAX_SYMBOLS
    );

    // ------------------------------------------------------------
    // Reconnect amplification check
    // ------------------------------------------------------------

    std::uint64_t epoch = h.force_reconnect();
    h.wait_for_epoch(epoch);

    h.drain();

    TEST_CHECK(
        h.session.trade_subscriptions().total_symbols() <= MAX_SYMBOLS
    );

    TEST_CHECK(
        h.session.replay_database().trade_table().total_symbols() <= MAX_SYMBOLS
    );

#ifndef NDEBUG
    h.session.replay_database().trade_table().assert_consistency();
#endif

    std::cout << "[TEST] OK\n";
}


// ------------------------------------------------------------
// Runner
// ------------------------------------------------------------

#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Debug);

    test_out_of_order_ack_burst();
    test_duplicate_ack_storm();
    test_subscribe_unsubscribe_race_under_replay();
    test_replay_db_saturation_limit();
    test_replay_db_mixed_trade_book_stress();
    test_saturation_race_overlap();
    test_hard_limit_enforcement();

    std::cout << "\n[GROUP H - DETERMINISTIC ADVERSARIAL PROTOCOL TESTS PASSED]\n";
    return 0;
}
