#include <cassert>
#include <iostream>
#include <string_view>

#include "simdjson.h"

#include "wirekrak/core/protocol/kraken/parser/rejection_notice.hpp"

using namespace wirekrak::core::protocol::kraken;

/*
================================================================================
Kraken Rejection Notice Parser — Unit Tests
================================================================================

These tests validate parsing of Kraken WebSocket rejection / error notices.

Design goals enforced by this test suite:
  • Required vs optional field correctness
  • Deterministic parse behavior (true / false)
  • No exceptions on malformed input
  • Proper optional reset between parses
  • Acceptance of real-world Kraken error payloads
  • Rejection of invalid or malformed fields

The parser is tested in isolation. Routing by method/channel is assumed
to have already occurred upstream.
================================================================================
*/

static bool parse(std::string_view json, schema::rejection::Notice& out) {
    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());
    return parser::rejection_notice::parse(doc.value(), out) == parser::Result::Parsed;
}

// ------------------------------------------------------------
// POSITIVE CASES
// ------------------------------------------------------------

void test_rejection_notice_minimal() {
    std::cout << "[TEST] Rejection notice (minimal)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "error": "Already subscribed"
    }
    )json";

    schema::rejection::Notice notice{};
    assert(parse(json, notice));

    assert(notice.error == "Already subscribed");
    assert(!notice.req_id.has());
    assert(!notice.symbol.has());
    assert(!notice.time_in.has());
    assert(!notice.time_out.has());

    std::cout << "[TEST] OK\n";
}

void test_rejection_notice_full_payload() {
    std::cout << "[TEST] Rejection notice (full payload)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "error": "Already subscribed",
        "req_id": 42,
        "symbol": "BTC/USD",
        "time_in":  "2025-12-20T07:39:28.809188Z",
        "time_out": "2025-12-20T07:39:28.809200Z"
    }
    )json";

    schema::rejection::Notice notice{};
    assert(parse(json, notice));

    assert(notice.error == "Already subscribed");
    assert(notice.req_id.has() && notice.req_id.value() == 42);
    assert(notice.symbol.has() && notice.symbol.value() == "BTC/USD");
    assert(notice.time_in.has());
    assert(notice.time_out.has());

    std::cout << "[TEST] OK\n";
}

void test_rejection_notice_without_symbol() {
    std::cout << "[TEST] Rejection notice (no symbol)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "error": "Invalid request",
        "req_id": 7
    }
    )json";

    schema::rejection::Notice notice{};
    assert(parse(json, notice));

    assert(notice.error == "Invalid request");
    assert(notice.req_id.has());
    assert(!notice.symbol.has());

    std::cout << "[TEST] OK\n";
}

// ------------------------------------------------------------
// FAILURE CASES
// ------------------------------------------------------------

void test_rejection_notice_missing_error() {
    std::cout << "[TEST] Rejection notice (missing error)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "req_id": 1
    }
    )json";

    schema::rejection::Notice notice{};
    assert(!parse(json, notice));

    std::cout << "[TEST] OK\n";
}

void test_rejection_notice_invalid_req_id_type() {
    std::cout << "[TEST] Rejection notice (invalid req_id type)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "error": "Bad request",
        "req_id": "not-a-number"
    }
    )json";

    schema::rejection::Notice notice{};
    assert(!parse(json, notice));

    std::cout << "[TEST] OK\n";
}

void test_rejection_notice_invalid_symbol_empty_string() {
    std::cout << "[TEST] Rejection notice (empty symbol string)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "error": "Bad symbol",
        "symbol": ""
    }
    )json";

    schema::rejection::Notice notice{};
    assert(!parse(json, notice));

    std::cout << "[TEST] OK\n";
}

void test_rejection_notice_invalid_time_format() {
    std::cout << "[TEST] Rejection notice (invalid timestamp)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "error": "Bad timestamp",
        "time_in": "not-a-timestamp"
    }
    )json";

    schema::rejection::Notice notice{};
    assert(!parse(json, notice));

    std::cout << "[TEST] OK\n";
}

// ------------------------------------------------------------
// ROOT TYPE VALIDATION
// ------------------------------------------------------------

void test_rejection_notice_root_not_object_array() {
    std::cout << "[TEST] Rejection notice (root is array)..." << std::endl;

    constexpr std::string_view json = R"json(
    [
        { "error": "bad" }
    ]
    )json";

    schema::rejection::Notice notice{};
    assert(!parse(json, notice));

    std::cout << "[TEST] OK\n";
}

void test_rejection_notice_root_not_object_string() {
    std::cout << "[TEST] Rejection notice (root is string)..." << std::endl;

    constexpr std::string_view json = R"json(
    "not-an-object"
    )json";

    schema::rejection::Notice notice{};
    assert(!parse(json, notice));

    std::cout << "[TEST] OK\n";
}

void test_rejection_notice_root_not_object_number() {
    std::cout << "[TEST] Rejection notice (root is number)..." << std::endl;

    constexpr std::string_view json = R"json(
    12345
    )json";

    schema::rejection::Notice notice{};
    assert(!parse(json, notice));

    std::cout << "[TEST] OK\n";
}

// ------------------------------------------------------------
// MAIN
// ------------------------------------------------------------

int main() {
    // root validation
    test_rejection_notice_root_not_object_array();
    test_rejection_notice_root_not_object_string();
    test_rejection_notice_root_not_object_number();

    // positive cases
    test_rejection_notice_minimal();
    test_rejection_notice_full_payload();
    test_rejection_notice_without_symbol();

    // negative cases
    test_rejection_notice_missing_error();
    test_rejection_notice_invalid_req_id_type();
    test_rejection_notice_invalid_symbol_empty_string();
    test_rejection_notice_invalid_time_format();

    std::cout << "[TEST] ALL REJECTION NOTICE PARSER TESTS PASSED!\n";
    return 0;
}
