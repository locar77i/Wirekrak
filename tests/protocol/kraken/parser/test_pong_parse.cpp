#include <cassert>
#include <iostream>
#include <string_view>

#include "simdjson.h"

#include "wirekrak/protocol/kraken/parser/system/pong.hpp"

using namespace wirekrak::protocol::kraken;

/*
================================================================================
Kraken Pong Response Parser — Unit Tests
================================================================================

These tests validate parsing of Kraken WebSocket "pong" responses.

IMPORTANT:
Kraken sends pong messages in two formats:

1) Heartbeat pong (observed in production):
{
  "method": "pong",
  "req_id": integer,
  "time_in": RFC3339 string,
  "time_out": RFC3339 string
}

2) Request-style pong (documented schema):
{
  "method": "pong",
  "success": true,
  "result": { "warnings": [string, ...] },
  "req_id": integer
}

This test suite validates correct handling of BOTH forms.

Design goals enforced:
  • Robust handling of real Kraken behavior
  • Strict enforcement when success/error semantics are explicit
  • Deterministic parse behavior (true / false only)
================================================================================
*/

// ============================================================================
// SUCCESS CASES — HEARTBEAT STYLE (NO success FIELD)
// ============================================================================

void test_pong_heartbeat_minimal() {
    std::cout << "[TEST] Pong response (heartbeat minimal)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "pong"
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    system::Pong pong{};
    bool ok = parser::system::pong::parse(doc.value(), pong);

    assert(ok);
    assert(!pong.success.has());          // implicit success
    assert(pong.warnings.empty());
    assert(!pong.error.has());

    std::cout << "[TEST] OK\n";
}

void test_pong_heartbeat_with_timestamps() {
    std::cout << "[TEST] Pong response (heartbeat with timestamps)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "pong",
        "req_id": 1,
        "time_in": "2025-12-19T18:26:27.595864Z",
        "time_out": "2025-12-19T18:26:27.595887Z"
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    system::Pong pong{};
    bool ok = parser::system::pong::parse(doc.value(), pong);

    assert(ok);
    assert(!pong.success.has());
    assert(pong.req_id.has());
    assert(pong.time_in.has());
    assert(pong.time_out.has());
    assert(pong.warnings.empty());

    std::cout << "[TEST] OK\n";
}

// ============================================================================
// SUCCESS CASES — REQUEST-STYLE (success = true)
// ============================================================================

void test_pong_success_minimal() {
    std::cout << "[TEST] Pong response (success minimal)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "pong",
        "success": true,
        "result": {}
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    system::Pong pong{};
    bool ok = parser::system::pong::parse(doc.value(), pong);

    assert(ok);
    assert(pong.success.has());
    assert(pong.success.value());
    assert(pong.warnings.empty());
    assert(!pong.error.has());

    std::cout << "[TEST] OK\n";
}

void test_pong_success_full() {
    std::cout << "[TEST] Pong response (success full payload)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "pong",
        "success": true,
        "req_id": 42,
        "result": {
            "warnings": ["deprecated field"]
        },
        "time_in": "2022-12-25T09:30:59.123456Z",
        "time_out": "2022-12-25T09:30:59.223456Z"
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    system::Pong pong{};
    bool ok = parser::system::pong::parse(doc.value(), pong);

    assert(ok);
    assert(pong.success.has());
    assert(pong.success.value());
    assert(pong.req_id.has());
    assert(pong.warnings.size() == 1);
    assert(pong.time_in.has());
    assert(pong.time_out.has());

    std::cout << "[TEST] OK\n";
}

// ============================================================================
// ERROR CASES — success = false
// ============================================================================

void test_pong_error_minimal() {
    std::cout << "[TEST] Pong response (error minimal)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "pong",
        "success": false,
        "error": "Invalid request"
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    system::Pong pong{};
    bool ok = parser::system::pong::parse(doc.value(), pong);

    assert(ok);
    assert(pong.success.has());
    assert(!pong.success.value());
    assert(pong.error.has());

    std::cout << "[TEST] OK\n";
}

// ============================================================================
// NEGATIVE CASES — MUST FAIL
// ============================================================================

void test_pong_success_missing_result() {
    std::cout << "[TEST] Pong response (success missing result)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "pong",
        "success": true
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    system::Pong pong{};
    assert(!parser::system::pong::parse(doc.value(), pong));

    std::cout << "[TEST] OK\n";
}

void test_pong_error_missing_error_field() {
    std::cout << "[TEST] Pong response (error missing error field)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "pong",
        "success": false
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    system::Pong pong{};
    assert(!parser::system::pong::parse(doc.value(), pong));

    std::cout << "[TEST] OK\n";
}

void test_pong_invalid_warnings_type() {
    std::cout << "[TEST] Pong response (invalid warnings type)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "pong",
        "success": true,
        "result": {
            "warnings": "not-an-array"
        }
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    system::Pong pong{};
    assert(!parser::system::pong::parse(doc.value(), pong));

    std::cout << "[TEST] OK\n";
}

void test_pong_root_not_object() {
    std::cout << "[TEST] Pong response (root not object)..." << std::endl;

    constexpr std::string_view json = R"json(
    ["pong"]
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    system::Pong pong{};
    assert(!parser::system::pong::parse(doc.value(), pong));

    std::cout << "[TEST] OK\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    // Heartbeat style
    test_pong_heartbeat_minimal();
    test_pong_heartbeat_with_timestamps();

    // Request-style success
    test_pong_success_minimal();
    test_pong_success_full();

    // Error
    test_pong_error_minimal();

    // Negative
    test_pong_success_missing_result();
    test_pong_error_missing_error_field();
    test_pong_invalid_warnings_type();
    test_pong_root_not_object();

    std::cout << "[TEST] ALL PONG RESPONSE PARSER TESTS PASSED!\n";
    return 0;
}
