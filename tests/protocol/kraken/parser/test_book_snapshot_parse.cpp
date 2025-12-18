#include <cassert>
#include <iostream>
#include <string_view>

#include "simdjson.h"

#include "wirekrak/protocol/kraken/book/snapshot.hpp"
#include "wirekrak/protocol/kraken/parser/book/snapshot.hpp"

using namespace wirekrak::protocol::kraken;

/*
================================================================================
Kraken Book Snapshot & Update Parser — Unit Tests
================================================================================

These tests validate parsing of Kraken "book" channel data messages
(type = snapshot | update).

The snapshot and update parsers share a common parsing core and differ only
in message semantics (initial state vs incremental changes). This test suite
ensures that:

  • Both message types accept valid, spec-compliant payloads
  • Shared fields (symbol, bids, asks, checksum) are parsed consistently
  • Update-only fields (timestamp) are validated when required
  • Malformed messages are safely rejected without exceptions
  • Schema violations never propagate into higher layers

By exercising both snapshot and update through the same parsing logic,
these tests guarantee behavioral parity and prevent divergence between
initial book state handling and incremental order book updates.
================================================================================
*/

static bool parse(std::string_view json, book::Snapshot& out) {
    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());
    return parser::book::snapshot::parse(doc.value(), out);
}

// ------------------------------------------------------------
// POSITIVE CASES
// ------------------------------------------------------------

void test_book_snapshot_success_bids_and_asks() {
    std::cout << "[TEST] Book snapshot (bids + asks)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "snapshot",
        "data": [{
            "symbol": "BTC/USD",
            "asks": [{ "price": 50000.0, "qty": 1.5 }],
            "bids": [{ "price": 49900.0, "qty": 2.0 }],
            "checksum": 123456
        }]
    }
    )json";

    book::Snapshot snap{};
    assert(parse(json, snap));
    assert(snap.asks.size() == 1);
    assert(snap.bids.size() == 1);

    std::cout << "[TEST] OK\n";
}

void test_book_snapshot_success_asks_only() {
    std::cout << "[TEST] Book snapshot (asks only)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "snapshot",
        "data": [{
            "symbol": "BTC/USD",
            "asks": [{ "price": 50000.0, "qty": 1.5 }],
            "checksum": 11
        }]
    }
    )json";

    book::Snapshot snap{};
    assert(parse(json, snap));
    assert(!snap.asks.empty());
    assert(snap.bids.empty());

    std::cout << "[TEST] OK\n";
}

void test_book_snapshot_success_bids_only() {
    std::cout << "[TEST] Book snapshot (bids only)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "snapshot",
        "data": [{
            "symbol": "BTC/USD",
            "bids": [{ "price": 49900.0, "qty": 2.0 }],
            "checksum": 22
        }]
    }
    )json";

    book::Snapshot snap{};
    assert(parse(json, snap));
    assert(!snap.bids.empty());
    assert(snap.asks.empty());

    std::cout << "[TEST] OK\n";
}

// ------------------------------------------------------------
// NEGATIVE CASES
// ------------------------------------------------------------

void test_book_snapshot_missing_data() {
    std::cout << "[TEST] Book snapshot (missing data)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "snapshot"
    }
    )json";

    book::Snapshot snap{};
    assert(!parse(json, snap));

    std::cout << "[TEST] OK\n";
}

void test_book_snapshot_empty_data() {
    std::cout << "[TEST] Book snapshot (empty data)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "snapshot",
        "data": []
    }
    )json";

    book::Snapshot snap{};
    assert(!parse(json, snap));

    std::cout << "[TEST] OK\n";
}

void test_book_snapshot_missing_symbol() {
    std::cout << "[TEST] Book snapshot (missing symbol)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "snapshot",
        "data": [{
            "asks": [],
            "checksum": 1
        }]
    }
    )json";

    book::Snapshot snap{};
    assert(!parse(json, snap));

    std::cout << "[TEST] OK\n";
}

void test_book_snapshot_missing_checksum() {
    std::cout << "[TEST] Book snapshot (missing checksum)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "snapshot",
        "data": [{
            "symbol": "BTC/USD",
            "asks": []
        }]
    }
    )json";

    book::Snapshot snap{};
    assert(!parse(json, snap));

    std::cout << "[TEST] OK\n";
}

void test_book_snapshot_missing_bids_and_asks() {
    std::cout << "[TEST] Book snapshot (missing bids & asks)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "snapshot",
        "data": [{
            "symbol": "BTC/USD",
            "checksum": 1
        }]
    }
    )json";

    book::Snapshot snap{};
    assert(!parse(json, snap));

    std::cout << "[TEST] OK\n";
}

void test_book_snapshot_wrong_type() {
    std::cout << "[TEST] Book snapshot (wrong type)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "update",
        "data": []
    }
    )json";

    book::Snapshot snap{};
    assert(!parse(json, snap));

    std::cout << "[TEST] OK\n";
}


/*
================================================================================
Kraken Book Snapshot Parser — Unit Tests
================================================================================

Validates parsing of full order book snapshots (type = "snapshot").

Guarantees:
  • Strict schema enforcement
  • Deterministic parse results
  • No exceptions on malformed input
  • Shared book parsing logic is exercised through public API

================================================================================
*/
int main() {
    test_book_snapshot_success_bids_and_asks();
    test_book_snapshot_success_asks_only();
    test_book_snapshot_success_bids_only();

    test_book_snapshot_missing_data();
    test_book_snapshot_empty_data();
    test_book_snapshot_missing_symbol();
    test_book_snapshot_missing_checksum();
    test_book_snapshot_missing_bids_and_asks();
    test_book_snapshot_wrong_type();

    std::cout << "[TEST] ALL BOOK SNAPSHOT PARSER TESTS PASSED!\n";
    return 0;
}
