#include <cassert>
#include <iostream>
#include <string_view>

#include "simdjson.h"

#include "wirekrak/protocol/kraken/parser/trade/unsubscribe_ack.hpp"

using namespace wirekrak::protocol::kraken;

// -----------------------------------------------------------------------------
// Helper
// -----------------------------------------------------------------------------

static bool parse(std::string_view json, schema::trade::UnsubscribeAck& out) {
    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());
    return parser::trade::unsubscribe_ack::parse(doc.value(), out);
}

/*
================================================================================
Kraken Trade Unsubscribe ACK Parser — Unit Tests
================================================================================

These tests validate parsing of Kraken WebSocket "trade unsubscribe
acknowledgment" messages.

The unsubscribe ACK shares its schema with subscribe ACK, minus
subscribe-only fields (snapshot, warnings).

This suite guarantees:
  • Strict schema validation
  • Correct success / failure branching
  • No exceptions on malformed input
  • Safe behavior under invalid JSON shapes
================================================================================
*/

// ------------------------------------------------------------
// POSITIVE CASES
// ------------------------------------------------------------

void test_trade_unsubscribe_ack_success_minimal() {
    std::cout << "[TEST] Trade unsubscribe ack (success, minimal)...\n";

    constexpr std::string_view json = R"json(
    {
        "success": true,
        "result": {
            "symbol": "BTC/USD"
        }
    }
    )json";

    schema::trade::UnsubscribeAck ack{};
    assert(parse(json, ack));

    assert(ack.success == true);
    assert(ack.symbol == "BTC/USD");
    assert(!ack.error.has());
    assert(!ack.req_id.has());

    std::cout << "[TEST] OK\n";
}

void test_trade_unsubscribe_ack_success_full() {
    std::cout << "[TEST] Trade unsubscribe ack (success, full)...\n";

    constexpr std::string_view json = R"json(
    {
        "success": true,
        "req_id": 7,
        "time_in":  "2022-12-25T09:30:59.123456Z",
        "time_out": "2022-12-25T09:30:59.223456Z",
        "result": {
            "symbol": "ETH/USD"
        }
    }
    )json";

    schema::trade::UnsubscribeAck ack{};
    assert(parse(json, ack));

    assert(ack.success == true);
    assert(ack.symbol == "ETH/USD");
    assert(ack.req_id.has() && ack.req_id.value() == 7);
    assert(ack.time_in.has());
    assert(ack.time_out.has());

    std::cout << "[TEST] OK\n";
}

// ------------------------------------------------------------
// FAILURE CASES
// ------------------------------------------------------------

void test_trade_unsubscribe_ack_error_case() {
    std::cout << "[TEST] Trade unsubscribe ack (error)...\n";

    constexpr std::string_view json = R"json(
    {
        "success": false,
        "error": "not subscribed"
    }
    )json";

    schema::trade::UnsubscribeAck ack{};
    assert(parse(json, ack));

    assert(ack.success == false);
    assert(ack.error.has());
    assert(ack.error.value() == "not subscribed");

    std::cout << "[TEST] OK\n";
}

void test_trade_unsubscribe_ack_missing_success() {
    std::cout << "[TEST] Trade unsubscribe ack (missing success)...\n";

    constexpr std::string_view json = R"json(
    {
        "result": { "symbol": "BTC/USD" }
    }
    )json";

    schema::trade::UnsubscribeAck ack{};
    assert(!parse(json, ack));

    std::cout << "[TEST] OK\n";
}

void test_trade_unsubscribe_ack_success_missing_result() {
    std::cout << "[TEST] Trade unsubscribe ack (success, missing result)...\n";

    constexpr std::string_view json = R"json(
    {
        "success": true
    }
    )json";

    schema::trade::UnsubscribeAck ack{};
    assert(!parse(json, ack));

    std::cout << "[TEST] OK\n";
}

void test_trade_unsubscribe_ack_missing_symbol() {
    std::cout << "[TEST] Trade unsubscribe ack (missing symbol)...\n";

    constexpr std::string_view json = R"json(
    {
        "success": true,
        "result": {}
    }
    )json";

    schema::trade::UnsubscribeAck ack{};
    assert(!parse(json, ack));

    std::cout << "[TEST] OK\n";
}

// ------------------------------------------------------------
// ROOT TYPE VALIDATION
// ------------------------------------------------------------

void test_trade_unsubscribe_ack_root_not_object() {
    std::cout << "[TEST] Trade unsubscribe ack (root not object)...\n";

    constexpr std::string_view json = R"json(42)json";

    schema::trade::UnsubscribeAck ack{};
    assert(!parse(json, ack));

    std::cout << "[TEST] OK\n";
}

// ------------------------------------------------------------
// MAIN
// ------------------------------------------------------------

int main() {
    test_trade_unsubscribe_ack_root_not_object();

    test_trade_unsubscribe_ack_success_minimal();
    test_trade_unsubscribe_ack_success_full();

    test_trade_unsubscribe_ack_error_case();
    test_trade_unsubscribe_ack_missing_success();
    test_trade_unsubscribe_ack_success_missing_result();
    test_trade_unsubscribe_ack_missing_symbol();

    std::cout << "[TEST] ALL TRADE UNSUBSCRIBE ACK PARSER TESTS PASSED!\n";
    return 0;
}
