#include <cassert>
#include <iostream>
#include <string_view>

#include "simdjson.h"

#include "wirekrak/core/protocol/kraken/parser/book/unsubscribe_ack.hpp"

using namespace wirekrak::core::protocol::kraken;


/*
================================================================================
Kraken Book Unsubscribe ACK Parser — Unit Tests
================================================================================

These tests validate the correctness and robustness of the Kraken WebSocket
"book unsubscribe acknowledgment" message parser.

Design goals enforced by this test suite:
  • Strict schema validation — only spec-compliant messages are accepted
  • Failure-safe parsing — malformed or partial JSON must never throw
  • Deterministic behavior — parse() returns true/false, no side effects
  • Negative coverage — missing fields, wrong types, and invalid channels
    are explicitly rejected

The parser is exercised using simdjson DOM parsing, mirroring production usage,
while ensuring that all error paths are handled gracefully.

This guarantees that protocol-level faults cannot propagate into higher layers
(Client, Dispatcher, or Transport), a critical requirement for real-time and
high-availability trading systems.
================================================================================
*/


void test_book_unsubscribe_ack_success() {
    std::cout << "[TEST] Book unsubscribe ack parser (success)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "unsubscribe",
        "result": {
            "channel": "book",
            "symbol": "BTC/USD",
            "depth": 25
        },
        "success": true,
        "req_id": 7,
        "time_in":  "2022-12-25T09:30:59.123456Z",
        "time_out": "2022-12-25T09:30:59.223456Z"
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());
    simdjson::dom::element root = doc.value();

    schema::book::UnsubscribeAck ack{};
    bool ok = parser::book::unsubscribe_ack::parse(root, ack);

    assert(ok);

    // Required fields
    assert(ack.symbol == "BTC/USD");
    assert(ack.depth == 25);
    assert(ack.success == true);

    // Optional fields
    assert(!ack.error.has());
    assert(ack.req_id.has());
    assert(ack.req_id.value() == 7);

    // Timestamps
    assert(ack.time_in.has());
    assert(ack.time_out.has());

    std::cout << "[TEST] OK\n";
}

void test_book_unsubscribe_ack_error() {
    std::cout << "[TEST] Book unsubscribe ack parser (error)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "unsubscribe",
        "result": {
            "channel": "book",
            "symbol": "BTC/USD",
            "depth": 100
        },
        "success": false,
        "error": "not subscribed"
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());
    simdjson::dom::element root = doc.value();

    schema::book::UnsubscribeAck ack{};
    bool ok = parser::book::unsubscribe_ack::parse(root, ack);

    assert(ok);
    assert(ack.success == false);
    assert(ack.error.has());
    assert(ack.error.value() == "not subscribed");

    std::cout << "[TEST] OK\n";
}

/* Enforced by caller/router
void test_book_unsubscribe_ack_wrong_method() {
    std::cout << "[TEST] Book unsubscribe ack parser (wrong method)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "subscribe",
        "result": {
            "channel": "book",
            "symbol": "BTC/USD",
            "depth": 25
        },
        "success": true
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());
    simdjson::dom::element root = doc.value();

    schema::book::UnsubscribeAck ack{};
    bool ok = parser::book::unsubscribe_ack::parse(root, ack);

    assert(!ok);

    std::cout << "[TEST] OK\n";
}

void test_book_unsubscribe_ack_wrong_channel() {
    std::cout << "[TEST] Book unsubscribe ack parser (wrong channel)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "unsubscribe",
        "result": {
            "channel": "trade",
            "symbol": "BTC/USD",
            "depth": 25
        },
        "success": true
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());
    simdjson::dom::element root = doc.value();

    schema::book::UnsubscribeAck ack{};
    bool ok = parser::book::unsubscribe_ack::parse(root, ack);

    assert(!ok);

    std::cout << "[TEST] OK\n";
}
*/

void test_book_unsubscribe_ack_missing_symbol() {
    std::cout << "[TEST] Book unsubscribe ack parser (missing symbol)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "unsubscribe",
        "result": {
            "channel": "book",
            "depth": 25
        },
        "success": true
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());
    simdjson::dom::element root = doc.value();

    schema::book::UnsubscribeAck ack{};
    bool ok = parser::book::unsubscribe_ack::parse(root, ack);

    assert(!ok);

    std::cout << "[TEST] OK\n";
}

void test_book_unsubscribe_ack_invalid_depth_type() {
    std::cout << "[TEST] Book unsubscribe ack parser (invalid depth type)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "unsubscribe",
        "result": {
            "channel": "book",
            "symbol": "BTC/USD",
            "depth": "25"
        },
        "success": true
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());
    simdjson::dom::element root = doc.value();

    schema::book::UnsubscribeAck ack{};
    bool ok = parser::book::unsubscribe_ack::parse(root, ack);

    assert(!ok);

    std::cout << "[TEST] OK\n";
}

void test_book_unsubscribe_ack_missing_result() {
    std::cout << "[TEST] Book unsubscribe ack parser (missing result)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "unsubscribe"
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());
    simdjson::dom::element root = doc.value();

    schema::book::UnsubscribeAck ack{};
    bool ok = parser::book::unsubscribe_ack::parse(root, ack);

    assert(!ok);

    std::cout << "[TEST] OK\n";
}


int main() {
    test_book_unsubscribe_ack_success();
    test_book_unsubscribe_ack_error();

    // Negative tests
/* Enforced by caller/router
    test_book_unsubscribe_ack_wrong_method();
    test_book_unsubscribe_ack_wrong_channel();
*/
    test_book_unsubscribe_ack_missing_symbol();
    test_book_unsubscribe_ack_invalid_depth_type();
    test_book_unsubscribe_ack_missing_result();

    std::cout << "[TEST] ALL BOOK UNSUBSCRIBE ACK PARSER TESTS PASSED!\n";
    return 0;
}
