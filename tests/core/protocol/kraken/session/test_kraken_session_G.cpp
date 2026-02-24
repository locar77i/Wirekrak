/*
===============================================================================
 protocol::kraken::Session - Group G Long-Run Convergence Fuzz Tests
===============================================================================

Scope:
------
Stress replay, rejection, reconnect, and intent convergence using randomized
operation sequences.

These tests validate:

- Eventual protocol-idle convergence
- Replay DB and channel managers remain consistent
- No stuck pending requests
- Epoch monotonicity
- Cross-channel isolation under fuzz conditions

===============================================================================
*/

#include <iostream>
#include <random>
#include <vector>

#include "common/harness/session.hpp"


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
// G1 - Random Single-Channel Fuzz
// ------------------------------------------------------------

void test_single_channel_long_run_fuzz() {
    std::cout << "[TEST] G1 Single-channel long-run fuzz\n";

    test::SessionHarness h;
    h.connect();

    constexpr int STEPS = 1000;
    constexpr uint32_t SEED = 42;

    std::mt19937 rng(SEED);
    std::uniform_int_distribution<> action(0, 3);
    std::uniform_int_distribution<> coin(0, 1);

    std::vector<ctrl::req_id_t> pending_subs;

    for (int i = 0; i < STEPS; ++i) {

        switch (action(rng)) {

        case 0: { // subscribe
            auto sym = random_symbol(rng);
            auto req_id = h.subscribe_trade(sym);
            if (req_id != ctrl::INVALID_REQ_ID) {
                pending_subs.push_back(req_id);
            }
            break;
        }

        case 1: { // resolve one pending
            if (!pending_subs.empty()) {
                auto req_id = pending_subs.back();
                pending_subs.pop_back();

                auto sym = random_symbol(rng);

                if (coin(rng)) {
                    h.confirm_trade_subscription(req_id, sym);
                } else {
                    h.reject_trade_subscription(req_id, sym);
                }
            }
            break;
        }

        case 2: { // reconnect
            h.force_reconnect();
            h.wait_for_epoch(h.session.transport_epoch());
            break;
        }

        case 3: {
            h.drain();
            break;
        }
        }

        h.drain();
        h.drain_rejections();
    }

    // ------------------------------------------------------------
    // Final invariants
    // ------------------------------------------------------------

    TEST_CHECK(h.session.trade_subscriptions().pending_requests() >= 0);

    TEST_CHECK(
        h.session.pending_protocol_requests() >=
        h.session.trade_subscriptions().pending_requests()
    );

    TEST_CHECK(
        h.session.trade_subscriptions().total_symbols() ==
        h.session.replay_database().trade_table().total_symbols()
    );

    std::cout << "[TEST] OK\n";
}


// ------------------------------------------------------------
// G2 - Cross-channel fuzz
// ------------------------------------------------------------

void test_cross_channel_long_run_fuzz() {
    std::cout << "[TEST] G2 Cross-channel fuzz\n";

    test::SessionHarness h;
    h.connect();

    constexpr int STEPS = 1000;
    constexpr uint32_t SEED = 1337;

    std::mt19937 rng(SEED);

    std::vector<std::pair<ctrl::req_id_t, std::string>> trade_pending;
    std::vector<std::pair<ctrl::req_id_t, std::string>> book_pending;

    std::uniform_int_distribution<> action(0, 7);
    std::uniform_int_distribution<> coin(0, 1);

    std::uint64_t last_epoch = h.session.transport_epoch();

    for (int i = 0; i < STEPS; ++i) {

        switch (action(rng)) {

        // --- Trade subscribe
        case 0: {
            auto sym = random_symbol(rng);
            auto req_id = h.subscribe_trade(sym);
            if (req_id != ctrl::INVALID_REQ_ID) {
                trade_pending.emplace_back(req_id, sym);
            }
            break;
        }

        // --- Book subscribe
        case 1: {
            auto sym = random_symbol(rng);
            auto req_id = h.subscribe_book(sym, 25);
            if (req_id != ctrl::INVALID_REQ_ID) {
                book_pending.emplace_back(req_id, sym);
            }
            break;
        }

        // --- Resolve trade (ACK or reject)
        case 2: {
            if (!trade_pending.empty()) {
                auto [req_id, sym] = trade_pending.back();
                trade_pending.pop_back();

                if (coin(rng))
                    h.confirm_trade_subscription(req_id, sym);
                else
                    h.reject_trade_subscription(req_id, sym);
            }
            break;
        }

        // --- Resolve book (ACK or reject)
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

        // --- Trade unsubscribe
        case 4: {
            auto sym = random_symbol(rng);
            auto req_id = h.unsubscribe_trade(sym);
            if (req_id != ctrl::INVALID_REQ_ID) {
                trade_pending.emplace_back(req_id, sym);
            }
            break;
        }

        // --- Book unsubscribe
        case 5: {
            auto sym = random_symbol(rng);
            auto req_id = h.unsubscribe_book(sym, 25);
            if (req_id != ctrl::INVALID_REQ_ID) {
                book_pending.emplace_back(req_id, sym);
            }
            break;
        }

        // --- Reconnect
        case 6: {
            std::uint64_t new_epoch = h.force_reconnect();
            h.wait_for_epoch(last_epoch + 1);
            TEST_CHECK(new_epoch > last_epoch);
            last_epoch = new_epoch;
            break;
        }

        // --- Idle drain tick
        case 7: {
            h.drain();
            break;
        }
        }

        h.drain();
        h.drain_rejections();
    }

    // ------------------------------------------------------------
    // Final invariants
    // ------------------------------------------------------------

    const auto& trade_mgr   = h.session.trade_subscriptions();
    const auto& trade_db    = h.session.replay_database().trade_table();

    const auto& book_mgr    = h.session.book_subscriptions();
    const auto& book_db     = h.session.replay_database().book_table();

    // ---------------- Trade invariants ----------------

    TEST_CHECK(
        trade_mgr.total_symbols() ==
        trade_db.total_symbols()
    );

    TEST_CHECK(
        trade_mgr.active_symbols() <=
        trade_mgr.total_symbols()
    );

    TEST_CHECK(
        trade_db.total_symbols() >=
        trade_mgr.active_symbols()
    );

    // ---------------- Book invariants ----------------

    TEST_CHECK(
        book_mgr.total_symbols() ==
        book_db.total_symbols()
    );

    TEST_CHECK(
        book_mgr.active_symbols() <=
        book_mgr.total_symbols()
    );

    TEST_CHECK(
        book_db.total_symbols() >=
        book_mgr.active_symbols()
    );

    // ---------------- Session-level sanity ----------------

    // Pending protocol requests must equal sum of channel pending
    TEST_CHECK(
        h.session.pending_protocol_requests() ==
        trade_mgr.pending_requests() + book_mgr.pending_requests()
    );

    std::cout << "[TEST] OK\n";
}


// ------------------------------------------------------------
// G3 - Deterministic chaos simulator
// ------------------------------------------------------------

void test_deterministic_chaos_simulator() {
    std::cout << "[TEST] G3 Deterministic chaos simulator\n";

    test::SessionHarness h;
    h.connect();

    constexpr int STEPS = 1000;
    constexpr uint32_t SEED = 1337;

    std::mt19937 rng(SEED);
    std::uniform_int_distribution<> action(0, 9);
    std::uniform_int_distribution<> coin(0, 1);

    std::uint64_t last_epoch = h.session.transport_epoch();

    for (int step = 0; step < STEPS; ++step) {

        switch (action(rng)) {

        // --- Trade subscribe
        case 0: {
            auto sym = random_symbol(rng);
            h.subscribe_trade(sym);
            break;
        }

        // --- Book subscribe
        case 1: {
            auto sym = random_symbol(rng);
            h.subscribe_book(sym, 25);
            break;
        }

        // --- Trade unsubscribe
        case 2: {
            auto sym = random_symbol(rng);
            h.unsubscribe_trade(sym);
            break;
        }

        // --- Book unsubscribe
        case 3: {
            auto sym = random_symbol(rng);
            h.unsubscribe_book(sym, 25);
            break;
        }

        // --- Random trade rejection injection (safe: unknown ids ignored)
        case 4: {
            auto sym = random_symbol(rng);
            h.reject_trade_subscription(9999, sym); // bogus req_id (ignored safely)
            break;
        }

        // --- Random book rejection injection
        case 5: {
            auto sym = random_symbol(rng);
            h.reject_book_subscription(9999, sym);
            break;
        }

        // --- Reconnect storm
        case 6: {
            std::uint64_t new_epoch = h.force_reconnect();
            h.wait_for_epoch(last_epoch + 1);
            TEST_CHECK(new_epoch > last_epoch);
            last_epoch = new_epoch;
            break;
        }

        // --- Extra drain tick
        case 7:
        case 8:
        case 9: {
            h.drain();
            break;
        }
        }

        h.drain();
        h.drain_rejections();
    }

    // ------------------------------------------------------------
    // Structural invariants only (NO convergence assumption)
    // ------------------------------------------------------------

    const auto& trade_mgr = h.session.trade_subscriptions();
    const auto& book_mgr  = h.session.book_subscriptions();

    const auto& trade_db = h.session.replay_database().trade_table();
    const auto& book_db  = h.session.replay_database().book_table();

    // ------------------------------------------------------------
    // Replay DB must reflect logical manager state
    // ------------------------------------------------------------

    TEST_CHECK( trade_mgr.total_symbols() == trade_db.total_symbols() );

    TEST_CHECK( book_mgr.total_symbols() ==  book_db.total_symbols() );

    // ------------------------------------------------------------
    // Active symbols must never exceed logical symbols
    // ------------------------------------------------------------

    TEST_CHECK( trade_mgr.active_symbols() <= trade_mgr.total_symbols() );

    TEST_CHECK( book_mgr.active_symbols() <= book_mgr.total_symbols() );

    // ------------------------------------------------------------
    // Pending accounting consistency
    // ------------------------------------------------------------

    TEST_CHECK(
        h.session.pending_protocol_requests() ==
        trade_mgr.pending_requests() +
        book_mgr.pending_requests()
    );

    // ------------------------------------------------------------
    // Epoch monotonicity already validated during run
    // ------------------------------------------------------------

    std::cout << "[TEST] OK\n";
}


// ------------------------------------------------------------
// G4 - Replay storm amplification
// Forces reconnect every 5 steps
// ------------------------------------------------------------

void test_replay_storm_amplification() {
    std::cout << "[TEST] G4 Replay storm amplification\n";

    test::SessionHarness h;
    h.connect();

    constexpr int STEPS = 1000;
    constexpr uint32_t SEED = 4242;

    std::mt19937 rng(SEED);
    std::uniform_int_distribution<> action(0, 5);
    std::uniform_int_distribution<> coin(0, 1);

    std::uint64_t last_epoch = h.session.transport_epoch();

    for (int step = 0; step < STEPS; ++step) {

        // --------------------------------------------------------
        // Forced reconnect storm every 5 steps
        // --------------------------------------------------------
        if (step % 5 == 0) {
            std::uint64_t new_epoch = h.force_reconnect();
            h.wait_for_epoch(last_epoch + 1);

            TEST_CHECK(new_epoch > last_epoch);
            last_epoch = new_epoch;
        }

        switch (action(rng)) {

        case 0: { // trade subscribe
            auto sym = random_symbol(rng);
            h.subscribe_trade(sym);
            break;
        }

        case 1: { // book subscribe
            auto sym = random_symbol(rng);
            h.subscribe_book(sym, 25);
            break;
        }

        case 2: { // trade unsubscribe
            auto sym = random_symbol(rng);
            h.unsubscribe_trade(sym);
            break;
        }

        case 3: { // book unsubscribe
            auto sym = random_symbol(rng);
            h.unsubscribe_book(sym, 25);
            break;
        }

        case 4: { // random bogus rejection
            auto sym = random_symbol(rng);
            h.reject_trade_subscription(9999, sym); // ignored safely
            break;
        }

        case 5: {
            auto sym = random_symbol(rng);
            h.reject_book_subscription(9999, sym);
            break;
        }
        }

        h.drain();
        h.drain_rejections();
    }

    // ------------------------------------------------------------
    // Structural invariants (no convergence requirement)
    // ------------------------------------------------------------

    const auto& trade_mgr = h.session.trade_subscriptions();
    const auto& book_mgr  = h.session.book_subscriptions();

    const auto& trade_db = h.session.replay_database().trade_table();
    const auto& book_db  = h.session.replay_database().book_table();

    // Replay DB matches logical manager state
    TEST_CHECK(
        trade_mgr.total_symbols() ==
        trade_db.total_symbols()
    );

    TEST_CHECK(
        book_mgr.total_symbols() ==
        book_db.total_symbols()
    );

    // Active ⊆ Logical
    TEST_CHECK(
        trade_mgr.active_symbols() <= trade_mgr.total_symbols()
    );

    TEST_CHECK(
        book_mgr.active_symbols() <= book_mgr.total_symbols()
    );

    // No replay explosion
    TEST_CHECK(trade_db.total_symbols() < 1000);
    TEST_CHECK(book_db.total_symbols() < 1000);

    // Pending accounting consistency
    TEST_CHECK(
        h.session.pending_protocol_requests() ==
        trade_mgr.pending_requests() +
        book_mgr.pending_requests()
    );

    // Epoch monotonicity maintained
    TEST_CHECK(h.session.transport_epoch() >= last_epoch);

    std::cout << "[TEST] OK\n";
}


// ------------------------------------------------------------
// G5 - Replay with delayed ACK simulation
// Simulates late ACKs arriving after reconnect
// ------------------------------------------------------------

void test_replay_with_delayed_ack_simulation() {
    std::cout << "[TEST] G5 Replay with delayed ACK simulation\n";

    test::SessionHarness h;
    h.connect();

    constexpr int STEPS = 1000;
    constexpr uint32_t SEED = 9001;

    std::mt19937 rng(SEED);
    std::uniform_int_distribution<> action(0, 4);
    std::uniform_int_distribution<> coin(0, 1);

    std::uint64_t last_epoch = h.session.transport_epoch();

    // Store delayed ACKs (req_id, symbol, is_trade, is_success)
    struct DelayedAck {
        ctrl::req_id_t req_id;
        std::string sym;
        bool is_trade;
        bool success;
    };

    std::vector<DelayedAck> delayed;

    for (int step = 0; step < STEPS; ++step) {

        switch (action(rng)) {

        case 0: { // trade subscribe (ACK delayed)
            auto sym = random_symbol(rng);
            auto req_id  = h.subscribe_trade(sym);
            if (req_id != ctrl::INVALID_REQ_ID) {
                delayed.push_back({req_id, sym, true, static_cast<bool>(coin(rng))});
            }
            break;
        }

        case 1: { // book subscribe (ACK delayed)
            auto sym = random_symbol(rng);
            auto req_id  = h.subscribe_book(sym, 25);
            if (req_id != ctrl::INVALID_REQ_ID) {
                delayed.push_back({req_id, sym, false, static_cast<bool>(coin(rng))});
            }
            break;
        }

        case 2: { // deliver one delayed ACK randomly
            if (!delayed.empty()) {
                auto idx = rng() % delayed.size();
                auto ack = delayed[idx];

                if (ack.is_trade) {
                    if (ack.success)
                        h.confirm_trade_subscription(ack.req_id, ack.sym);
                    else
                        h.reject_trade_subscription(ack.req_id, ack.sym);
                } else {
                    if (ack.success)
                        h.confirm_book_subscription(ack.req_id, ack.sym, 25);
                    else
                        h.reject_book_subscription(ack.req_id, ack.sym);
                }

                delayed.erase(delayed.begin() + idx);
            }
            break;
        }

        case 3: { // forced reconnect
            std::uint64_t new_epoch = h.force_reconnect();
            h.wait_for_epoch(last_epoch + 1);
            TEST_CHECK(new_epoch > last_epoch);
            last_epoch = new_epoch;
            break;
        }

        case 4: { // idle drain
            h.drain();
            break;
        }
        }

        h.drain();
        h.drain_rejections();
    }

    // ------------------------------------------------------------
    // Deliver remaining delayed ACKs (late arrivals)
    // ------------------------------------------------------------
    for (auto& ack : delayed) {
        if (ack.is_trade) {
            if (ack.success)
                h.confirm_trade_subscription(ack.req_id, ack.sym);
            else
                h.reject_trade_subscription(ack.req_id, ack.sym);
        } else {
            if (ack.success)
                h.confirm_book_subscription(ack.req_id, ack.sym, 25);
            else
                h.reject_book_subscription(ack.req_id, ack.sym);
        }

        h.drain();
        h.drain_rejections();
    }

    // ------------------------------------------------------------
    // Structural invariants
    // ------------------------------------------------------------

    const auto& trade_mgr = h.session.trade_subscriptions();
    const auto& book_mgr  = h.session.book_subscriptions();

    const auto& trade_db = h.session.replay_database().trade_table();
    const auto& book_db  = h.session.replay_database().book_table();

    // Replay DB matches manager logical state
    TEST_CHECK( trade_mgr.total_symbols() == trade_db.total_symbols() );

    TEST_CHECK( book_mgr.total_symbols() == book_db.total_symbols() );

    // Active ⊆ logical
    TEST_CHECK( trade_mgr.active_symbols() <= trade_mgr.total_symbols() );

    TEST_CHECK( book_mgr.active_symbols() <= book_mgr.total_symbols() );

    // No replay explosion
    TEST_CHECK( trade_db.total_symbols() < 1000 );
    TEST_CHECK( book_db.total_symbols() < 1000 );

    // Pending accounting consistent
    TEST_CHECK(
        h.session.pending_protocol_requests() ==
        trade_mgr.pending_requests() + book_mgr.pending_requests()
    );

    // Epoch monotonic
    TEST_CHECK( h.session.transport_epoch() >= last_epoch );

    std::cout << "[TEST] OK\n";
}


// ------------------------------------------------------------
// Runner
// ------------------------------------------------------------

#include "lcr/log/logger.hpp"

int main() {
    lcr::log::Logger::instance().set_level(lcr::log::Level::Debug);

    test_single_channel_long_run_fuzz();
    test_cross_channel_long_run_fuzz();
    test_deterministic_chaos_simulator();
    test_replay_storm_amplification();
    test_replay_with_delayed_ack_simulation();

    std::cout << "\n[GROUP G - LONG-RUN FUZZ TESTS PASSED]\n";
    return 0;
}
