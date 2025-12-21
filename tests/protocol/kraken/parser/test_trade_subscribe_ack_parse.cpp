#include <cassert>
#include <iostream>
#include <string_view>

#include "simdjson.h"

#include "wirekrak/protocol/kraken/parser/trade/subscribe_ack.hpp"

using namespace wirekrak::protocol::kraken;

/*
================================================================================
Kraken Trade Subscribe ACK Parser — Unit Tests
================================================================================

These tests validate parsing of Kraken WebSocket "trade subscribe acknowledgment"
messages.

Design goals enforced by this test suite:
  • Strict schema validation
  • Deterministic parse behavior (true / false)
  • No exceptions on malformed input
  • Clear separation between success and error paths
  • Optional fields parsed only when present
  • Parser remains safe under partial or invalid JSON

The parser is tested in isolation, assuming routing by channel/method
has already occurred upstream.
================================================================================
*/

static bool parse(std::string_view json, trade::SubscribeAck& out) {
    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());
    return parser::trade::subscribe_ack::parse(doc.value(), out);
}

// ------------------------------------------------------------
// POSITIVE CASES
// ------------------------------------------------------------

void test_trade_subscribe_ack_success_minimal() {
    std::cout << "[TEST] Trade subscribe ack (success, minimal)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "success": true,
        "result": {
            "symbol": "BTC/USD"
        }
    }
    )json";

    trade::SubscribeAck ack{};
    assert(parse(json, ack));

    assert(ack.success == true);
    assert(ack.symbol == "BTC/USD");
    assert(!ack.snapshot.has());
    assert(ack.warnings.empty());
    assert(!ack.error.has());
    assert(!ack.req_id.has());

    std::cout << "[TEST] OK\n";
}

void test_trade_subscribe_ack_success_full() {
    std::cout << "[TEST] Trade subscribe ack (success, full)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "success": true,
        "req_id": 42,
        "time_in":  "2022-12-25T09:30:59.123456Z",
        "time_out": "2022-12-25T09:30:59.223456Z",
        "result": {
            "symbol": "ETH/USD",
            "snapshot": true,
            "warnings": ["deprecated field"]
        }
    }
    )json";

    trade::SubscribeAck ack{};
    assert(parse(json, ack));

    assert(ack.success == true);
    assert(ack.symbol == "ETH/USD");
    assert(ack.snapshot.has() && ack.snapshot.value() == true);
    assert(!ack.warnings.empty());
    assert(ack.warnings[0] == "deprecated field");
    assert(ack.req_id.has() && ack.req_id.value() == 42);
    assert(ack.time_in.has());
    assert(ack.time_out.has());

    std::cout << "[TEST] OK\n";
}

// ------------------------------------------------------------
// FAILURE CASES
// ------------------------------------------------------------

void test_trade_subscribe_ack_error_case() {
    std::cout << "[TEST] Trade subscribe ack (error)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "success": false,
        "error": "invalid symbol"
    }
    )json";

    trade::SubscribeAck ack{};
    assert(parse(json, ack));

    assert(ack.success == false);
    assert(ack.error.has());
    assert(ack.error.value() == "invalid symbol");

    std::cout << "[TEST] OK\n";
}

void test_trade_subscribe_ack_missing_success() {
    std::cout << "[TEST] Trade subscribe ack (missing success)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "result": { "symbol": "BTC/USD" }
    }
    )json";

    trade::SubscribeAck ack{};
    assert(!parse(json, ack));

    std::cout << "[TEST] OK\n";
}

void test_trade_subscribe_ack_success_missing_result() {
    std::cout << "[TEST] Trade subscribe ack (success, missing result)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "success": true
    }
    )json";

    trade::SubscribeAck ack{};
    assert(!parse(json, ack));

    std::cout << "[TEST] OK\n";
}

void test_trade_subscribe_ack_missing_symbol() {
    std::cout << "[TEST] Trade subscribe ack (missing symbol)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "success": true,
        "result": {}
    }
    )json";

    trade::SubscribeAck ack{};
    assert(!parse(json, ack));

    std::cout << "[TEST] OK\n";
}

void test_trade_subscribe_ack_invalid_warnings_type() {
    std::cout << "[TEST] Trade subscribe ack (invalid warnings type)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "success": true,
        "result": {
            "symbol": "BTC/USD",
            "warnings": "not-an-array"
        }
    }
    )json";

    trade::SubscribeAck ack{};
    assert(!parse(json, ack));

    std::cout << "[TEST] OK\n";
}

void test_trade_subscribe_ack_root_not_object_array() {
    std::cout << "[TEST] Trade subscribe ack (root is array)..." << std::endl;

    constexpr std::string_view json = R"json(
    [
        { "success": true }
    ]
    )json";

    trade::SubscribeAck ack{};
    assert(!parse(json, ack));

    std::cout << "[TEST] OK\n";
}

void test_trade_subscribe_ack_root_not_object_string() {
    std::cout << "[TEST] Trade subscribe ack (root is string)..." << std::endl;

    constexpr std::string_view json = R"json(
    "not-an-object"
    )json";

    trade::SubscribeAck ack{};
    assert(!parse(json, ack));

    std::cout << "[TEST] OK\n";
}

void test_trade_subscribe_ack_root_not_object_number() {
    std::cout << "[TEST] Trade subscribe ack (root is number)..." << std::endl;

    constexpr std::string_view json = R"json(
    12345
    )json";

    trade::SubscribeAck ack{};
    assert(!parse(json, ack));

    std::cout << "[TEST] OK\n";
}


// ------------------------------------------------------------
// MAIN
// ------------------------------------------------------------

int main() {
    // root type validation
    test_trade_subscribe_ack_root_not_object_array();
    test_trade_subscribe_ack_root_not_object_string();
    test_trade_subscribe_ack_root_not_object_number();
    // positive cases
    test_trade_subscribe_ack_success_minimal();
    test_trade_subscribe_ack_success_full();
    // negative cases
    test_trade_subscribe_ack_error_case();
    test_trade_subscribe_ack_missing_success();
    test_trade_subscribe_ack_success_missing_result();
    test_trade_subscribe_ack_missing_symbol();
    test_trade_subscribe_ack_invalid_warnings_type();

    std::cout << "[TEST] ALL TRADE SUBSCRIBE ACK PARSER TESTS PASSED!\n";
    return 0;
}
