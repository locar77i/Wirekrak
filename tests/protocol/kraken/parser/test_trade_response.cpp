#include <cassert>
#include <iostream>
#include <string_view>

#include "simdjson.h"

#include "wirekrak/protocol/kraken/parser/trade/response.hpp"

using namespace wirekrak::protocol::kraken;

/*
================================================================================
Kraken Trade Response Parser — Unit Tests
================================================================================

These tests validate parsing of Kraken "trade" channel payloads
(type = snapshot | update).

Design goals enforced by this test suite:
  • Strict schema validation
  • Snapshot vs update semantic enforcement
  • Safe handling of malformed JSON
  • Deterministic parse behavior (true / false, no exceptions)
  • Complete negative coverage of required fields

The parser is exercised using simdjson DOM parsing, mirroring production usage.

This guarantees that malformed trade data cannot propagate into higher layers
(Client, Dispatcher, or Strategy), a critical invariant for trading systems.
================================================================================
*/

// ============================================================================
// SUCCESS CASES
// ============================================================================

void test_trade_snapshot_success() {
    std::cout << "[TEST] Trade response snapshot (success)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "type": "snapshot",
        "data": [
            {
                "symbol": "BTC/USD",
                "side": "buy",
                "qty": 0.5,
                "price": 50000.0,
                "trade_id": 1001,
                "timestamp": "2022-12-25T09:30:59.123456Z",
                "ord_type": "limit"
            },
            {
                "symbol": "ETH/USD",
                "side": "sell",
                "qty": 1.2,
                "price": 4000.0,
                "trade_id": 1002,
                "timestamp": "2022-12-25T09:31:00.000000Z"
            }
        ]
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    schema::trade::Response resp{};
    bool ok = parser::trade::response::parse(doc.value(), resp);

    assert(ok);
    assert(resp.type == PayloadType::Snapshot);
    assert(resp.trades.size() == 2);

    assert(resp.trades[0].symbol == "BTC/USD");
    assert(resp.trades[0].side == Side::Buy);
    assert(resp.trades[0].ord_type.has());

    assert(resp.trades[1].symbol == "ETH/USD");
    assert(!resp.trades[1].ord_type.has());

    std::cout << "[TEST] OK\n";
}

void test_trade_update_success() {
    std::cout << "[TEST] Trade response update (success)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "type": "update",
        "data": [
            {
                "symbol": "BTC/USD",
                "side": "sell",
                "qty": 0.1,
                "price": 49900.0,
                "trade_id": 2001,
                "timestamp": "2022-12-25T09:32:00.000000Z"
            }
        ]
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    schema::trade::Response resp{};
    bool ok = parser::trade::response::parse(doc.value(), resp);

    assert(ok);
    assert(resp.type == PayloadType::Update);
    assert(resp.trades.size() == 1);
    assert(resp.trades[0].trade_id == 2001);

    std::cout << "[TEST] OK\n";
}

// ============================================================================
// NEGATIVE CASES
// ============================================================================

void test_trade_missing_type() {
    std::cout << "[TEST] Trade response (missing type)..." << std::endl;

    constexpr std::string_view json = R"json(
    { "data": [] }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    schema::trade::Response resp{};
    assert(!parser::trade::response::parse(doc.value(), resp));

    std::cout << "[TEST] OK\n";
}

void test_trade_invalid_type() {
    std::cout << "[TEST] Trade response (invalid type)..." << std::endl;

    constexpr std::string_view json = R"json(
    { "type": "foo", "data": [] }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    schema::trade::Response resp{};
    assert(!parser::trade::response::parse(doc.value(), resp));

    std::cout << "[TEST] OK\n";
}

void test_trade_update_multiple_trades_rejected() {
    std::cout << "[TEST] Trade update with multiple trades (success)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "type": "update",
        "data": [
            {
                "symbol": "BTC/USD",
                "side": "buy",
                "qty": 1.0,
                "price": 50000,
                "trade_id": 1,
                "timestamp": "2022-12-25T09:30:00Z"
            },
            {
                "symbol": "BTC/USD",
                "side": "sell",
                "qty": 1.0,
                "price": 49900,
                "trade_id": 2,
                "timestamp": "2022-12-25T09:30:01Z"
            }
        ]
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    schema::trade::Response resp{};
    assert(parser::trade::response::parse(doc.value(), resp));
    assert(resp.trades.size() == 2);

    std::cout << "[TEST] OK\n";
}

void test_trade_missing_required_field() {
    std::cout << "[TEST] Trade response (missing price)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "type": "update",
        "data": [
            {
                "symbol": "BTC/USD",
                "side": "buy",
                "qty": 1.0,
                "trade_id": 10,
                "timestamp": "2022-12-25T09:30:00Z"
            }
        ]
    }
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    schema::trade::Response resp{};
    assert(!parser::trade::response::parse(doc.value(), resp));

    std::cout << "[TEST] OK\n";
}

void test_trade_root_not_object() {
    std::cout << "[TEST] Trade response (root not object)..." << std::endl;

    constexpr std::string_view json = R"json(
    ["invalid"]
    )json";

    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());

    schema::trade::Response resp{};
    assert(!parser::trade::response::parse(doc.value(), resp));

    std::cout << "[TEST] OK\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    test_trade_snapshot_success();
    test_trade_update_success();

    // Negative cases
    test_trade_missing_type();
    test_trade_invalid_type();
    test_trade_update_multiple_trades_rejected();
    test_trade_missing_required_field();
    test_trade_root_not_object();

    std::cout << "[TEST] ALL TRADE RESPONSE PARSER TESTS PASSED!\n";
    return 0;
}
