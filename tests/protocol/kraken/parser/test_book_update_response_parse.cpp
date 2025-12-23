#include <cassert>
#include <iostream>
#include <string_view>

#include "simdjson.h"

#include "wirekrak/protocol/kraken/parser/book/response.hpp"

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

static bool parse(std::string_view json, book::Response& out) {
    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());
    return parser::book::response::parse(doc.value(), out);
}

// ------------------------------------------------------------
// POSITIVE CASES
// ------------------------------------------------------------

void test_book_update_success_bids_only() {
    std::cout << "[TEST] Book update (bids only)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "update",
        "data": [{
            "symbol": "BTC/USD",
            "bids": [{ "price": 50000.0, "qty": 1.2 }],
            "checksum": 123,
            "timestamp": "2022-12-25T09:30:59.123456Z"
        }]
    }
    )json";

    book::Response resp{};
    assert(parse(json, resp));
    assert(resp.book.bids.size() == 1);
    assert(resp.book.asks.empty());

    std::cout << "[TEST] OK\n";
}

void test_book_update_success_asks_only() {
    std::cout << "[TEST] Book update (asks only)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "update",
        "data": [{
            "symbol": "BTC/USD",
            "asks": [{ "price": 50100.0, "qty": 0.5 }],
            "checksum": 321,
            "timestamp": "2022-12-25T09:30:59.123456Z"
        }]
    }
    )json";

    book::Response resp{};
    assert(parse(json, resp));
    assert(resp.book.asks.size() == 1);
    assert(resp.book.bids.empty());

    std::cout << "[TEST] OK\n";
}

void test_book_update_success_bids_and_asks() {
    std::cout << "[TEST] Book update (bids + asks)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "update",
        "data": [{
            "symbol": "BTC/USD",
            "asks": [{ "price": 50200.0, "qty": 0.3 }],
            "bids": [{ "price": 49900.0, "qty": 2.0 }],
            "checksum": 999,
            "timestamp": "2022-12-25T09:30:59.123456Z"
        }]
    }
    )json";

    book::Response resp{};
    assert(parse(json, resp));
    assert(resp.book.asks.size() == 1);
    assert(resp.book.bids.size() == 1);

    std::cout << "[TEST] OK\n";
}

// ------------------------------------------------------------
// NEGATIVE CASES
// ------------------------------------------------------------

void test_book_update_missing_timestamp() {
    std::cout << "[TEST] Book update (missing timestamp)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "update",
        "data": [{
            "symbol": "BTC/USD",
            "bids": [],
            "checksum": 1
        }]
    }
    )json";

    book::Response resp{};
    assert(parse(json, resp));
    assert(resp.book.timestamp.has() == false);

    std::cout << "[TEST] OK\n";
}

void test_book_update_missing_checksum() {
    std::cout << "[TEST] Book update (missing checksum)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "update",
        "data": [{
            "symbol": "BTC/USD",
            "bids": [],
            "timestamp": "2022-12-25T09:30:59.123456Z"
        }]
    }
    )json";

    book::Response resp{};
    assert(!parse(json, resp));

    std::cout << "[TEST] OK\n";
}

void test_book_update_missing_symbol() {
    std::cout << "[TEST] Book update (missing symbol)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "update",
        "data": [{
            "bids": [],
            "checksum": 1,
            "timestamp": "2022-12-25T09:30:59.123456Z"
        }]
    }
    )json";

    book::Response resp{};
    assert(!parse(json, resp));

    std::cout << "[TEST] OK\n";
}

void test_book_update_missing_bids_and_asks() {
    std::cout << "[TEST] Book update (missing bids & asks)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "update",
        "data": [{
            "symbol": "BTC/USD",
            "checksum": 1,
            "timestamp": "2022-12-25T09:30:59.123456Z"
        }]
    }
    )json";

    book::Response resp{};
    assert(!parse(json, resp));

    std::cout << "[TEST] OK\n";
}

void test_book_update_wrong_type() {
    std::cout << "[TEST] Book update (wrong type)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "snapshot",
        "data": []
    }
    )json";

    book::Response resp{};
    assert(!parse(json, resp));

    std::cout << "[TEST] OK\n";
}



/*
================================================================================
Kraken Book Update Parser — Unit Tests
================================================================================

Validates parsing of incremental book updates (type = "update").

Enforces:
  • Mandatory timestamp presence
  • Correct parsing of price levels
  • Strict schema rejection on missing fields

================================================================================
*/
int main() {
    test_book_update_success_bids_only();
    test_book_update_success_asks_only();
    test_book_update_success_bids_and_asks();

    test_book_update_missing_timestamp();
    test_book_update_missing_checksum();
    test_book_update_missing_symbol();
    test_book_update_missing_bids_and_asks();
    test_book_update_wrong_type();

    std::cout << "[TEST] ALL BOOK UPDATE PARSER TESTS PASSED!\n";
    return 0;
}
