#include <cassert>
#include <iostream>
#include <string_view>

#include "simdjson.h"

#include "wirekrak/protocol/kraken/system/pong.hpp"
#include "wirekrak/protocol/kraken/parser/system/pong.hpp"

using namespace wirekrak::protocol::kraken;

/*
================================================================================
Kraken Pong Response Parser — Unit Tests
================================================================================

These tests validate parsing of Kraken WebSocket "pong" responses.

Schema enforced (per Kraken spec):

SUCCESS RESPONSE:
{
  "method": "pong",
  "success": true,
  "result": { "warnings": [string, ...] },   // result REQUIRED on success
  "req_id": integer,                          // optional
  "time_in": RFC3339 string,                  // optional
  "time_out": RFC3339 string                  // optional
}

ERROR RESPONSE:
{
  "method": "pong",
  "success": false,
  "error": string,                            // REQUIRED on failure
  "req_id": integer                           // optional
}

Design goals enforced by this test suite:
  • Strict success vs error semantic enforcement
  • Required vs optional field correctness
  • Deterministic parse behavior (true / false only)
  • Safe rejection of malformed payloads
================================================================================
*/

// ============================================================================
// SUCCESS CASES
// ============================================================================

void test_pong_success_minimal() {
    std::cout << "[TEST] Pong response (success, minimal)..." << std::endl;

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

    schema::system::Pong pong{};
    bool ok = parser::system::pong::parse(doc.value(), pong);

    assert(ok);
    assert(pong.success);
    assert(!pong.req_id.has());
    assert(pong.warnings.empty());
    assert(!pong.time_in.has());
    assert(!pong.time_out.has());
    assert(!pong.error.has());

    std::cout << "[TEST] OK\n";
}

void test_pong_success_full() {
    std::cout << "[TEST] Pong response (success, full payload)..." << std::endl;

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

    schema::system::Pong pong{};
    bool ok = parser::system::pong::parse(doc.value(), pong);

    assert(ok);
    assert(pong.success);
    assert(pong.req_id.has());
    assert(pong.req_id.value() == 42);

    assert(pong.warnings.size() == 1);
    assert(pong.warnings[0] == "deprecated field");

    assert(pong.time_in.has());
    assert(pong.time_out.has());
    assert(!pong.error.has());

    std::cout << "[TEST] OK\n";
}

void test_pong_success_empty_warnings_array() {
    std::cout << "[TEST] Pong response (success, empty warnings array)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "pong",
        "success": true,
        "result": {
            "warnings": []
        }
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    schema::system::Pong pong{};
    bool ok = parser::system::pong::parse(doc.value(), pong);

    assert(ok);
    assert(pong.success);
    assert(pong.warnings.empty());

    std::cout << "[TEST] OK\n";
}

// ============================================================================
// ERROR CASES
// ============================================================================

void test_pong_error_minimal() {
    std::cout << "[TEST] Pong response (error, minimal)..." << std::endl;

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

    schema::system::Pong pong{};
    bool ok = parser::system::pong::parse(doc.value(), pong);

    assert(ok);
    assert(!pong.success);
    assert(pong.error.has());
    assert(pong.error.value() == "Invalid request");

    assert(pong.warnings.empty());
    assert(!pong.time_in.has());
    assert(!pong.time_out.has());

    std::cout << "[TEST] OK\n";
}

void test_pong_error_with_req_id() {
    std::cout << "[TEST] Pong response (error with req_id)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "method": "pong",
        "success": false,
        "req_id": 7,
        "error": "Rejected"
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    schema::system::Pong pong{};
    bool ok = parser::system::pong::parse(doc.value(), pong);

    assert(ok);
    assert(!pong.success);
    assert(pong.req_id.has());
    assert(pong.req_id.value() == 7);
    assert(pong.error.has());

    std::cout << "[TEST] OK\n";
}

// ============================================================================
// NEGATIVE CASES — MUST FAIL
// ============================================================================

void test_pong_missing_success() {
    std::cout << "[TEST] Pong response (missing success)..." << std::endl;

    constexpr std::string_view json = R"json(
    { "method": "pong" }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    schema::system::Pong pong{};
    assert(!parser::system::pong::parse(doc.value(), pong));

    std::cout << "[TEST] OK\n";
}

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

    schema::system::Pong pong{};
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

    schema::system::Pong pong{};
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

    schema::system::Pong pong{};
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

    schema::system::Pong pong{};
    assert(!parser::system::pong::parse(doc.value(), pong));

    std::cout << "[TEST] OK\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    // Success
    test_pong_success_minimal();
    test_pong_success_full();
    test_pong_success_empty_warnings_array();

    // Error
    test_pong_error_minimal();
    test_pong_error_with_req_id();

    // Negative
    test_pong_missing_success();
    test_pong_success_missing_result();
    test_pong_error_missing_error_field();
    test_pong_invalid_warnings_type();
    test_pong_root_not_object();

    std::cout << "[TEST] ALL PONG RESPONSE PARSER TESTS PASSED!\n";
    return 0;
}
