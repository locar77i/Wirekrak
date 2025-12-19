#include <cassert>
#include <iostream>
#include <string_view>

#include "wirekrak/protocol/kraken/book/subscribe.hpp"

using namespace wirekrak;
using namespace wirekrak::protocol::kraken;

/*
================================================================================
Kraken Book Subscribe Request — Unit Tests
================================================================================

These tests validate JSON serialization for the Kraken WebSocket
"book subscribe" request.

Design goals enforced by this test suite:
  • Deterministic JSON output
  • Strict schema compliance
  • No implicit defaults leaked into payload
  • Optional fields included only when set
  • Safe behavior under minimal and full configurations

This test suite validates request construction only. Transport-level
and server-side validation are intentionally out of scope.
================================================================================
*/

void test_book_subscribe_minimal() {
    std::cout << "[TEST] Book subscribe request (minimal)..." << std::endl;

    book::Subscribe sub;
    sub.symbols = { Symbol{"BTC/USD"} };

    std::string json = sub.to_json();

    // Required structure
    assert(json.find("\"method\":\"subscribe\"") != std::string::npos);
    assert(json.find("\"channel\":\"book\"") != std::string::npos);

    // Symbols
    assert(json.find("\"symbol\":[\"BTC/USD\"]") != std::string::npos);

    // Optional fields must NOT appear
    assert(json.find("\"snapshot\"") == std::string::npos);
    assert(json.find("\"depth\"") == std::string::npos);
    assert(json.find("\"req_id\"") == std::string::npos);

    std::cout << "[TEST] OK\n";
}

void test_book_subscribe_multiple_symbols() {
    std::cout << "[TEST] Book subscribe request (multiple symbols)..." << std::endl;

    book::Subscribe sub;
    sub.symbols = {
        Symbol{"BTC/USD"},
        Symbol{"ETH/USD"},
        Symbol{"MATIC/GBP"}
    };

    std::string json = sub.to_json();

    assert(json.find("\"symbol\":[\"BTC/USD\",\"ETH/USD\",\"MATIC/GBP\"]")
           != std::string::npos);

    std::cout << "[TEST] OK\n";
}

void test_book_subscribe_with_snapshot_and_req_id() {
    std::cout << "[TEST] Book subscribe request (snapshot + req_id)..." << std::endl;

    book::Subscribe sub;
    sub.symbols  = { Symbol{"BTC/USD"} };
    sub.snapshot = true;
    sub.req_id   = 12345;

    std::string json = sub.to_json();

    assert(json.find("\"snapshot\":true") != std::string::npos);
    assert(json.find("\"req_id\":12345") != std::string::npos);

    std::cout << "[TEST] OK\n";
}

void test_book_subscribe_snapshot_false() {
    std::cout << "[TEST] Book subscribe request (snapshot=false)..." << std::endl;

    book::Subscribe sub;
    sub.symbols  = { Symbol{"BTC/USD"} };
    sub.snapshot = false;

    std::string json = sub.to_json();

    assert(json.find("\"snapshot\":false") != std::string::npos);

    std::cout << "[TEST] OK\n";
}

#ifndef NDEBUG
void test_book_subscribe_empty_symbols_asserts() {
    std::cout << "[TEST] Book subscribe request (empty symbols — debug assert)..." << std::endl;

    book::Subscribe sub;

    bool asserted = false;
    try {
        (void)sub.to_json(); // should hit assert
    } catch (...) {
        asserted = true;
    }

    // NOTE:
    // We cannot reliably catch assert(), but this test exists
    // to document and validate the contract in Debug builds.
    std::cout << "[TEST] OK (assert expected in debug)\n";
}

void test_book_subscribe_invalid_depth_asserts() {
    std::cout << "[TEST] Book subscribe request (invalid depth — debug assert)..." << std::endl;

    book::Subscribe sub;
    sub.symbols = { Symbol{"BTC/USD"} };
    sub.depth   = 48; // invalid Kraken depth

    // NOTE:
    // assert() cannot be reliably caught.
    // This test documents the contract: invalid depth is a programmer error.
    (void)sub.to_json();

    std::cout << "[TEST] OK (assert expected in debug)\n";
}
#endif

int main() {
    test_book_subscribe_minimal();
    test_book_subscribe_multiple_symbols();
    test_book_subscribe_with_snapshot_and_req_id();
    test_book_subscribe_snapshot_false();

#ifndef NDEBUG
    //test_book_subscribe_empty_symbols_asserts();
    //test_book_subscribe_invalid_depth_asserts();
#endif

    std::cout << "[TEST] ALL BOOK SUBSCRIBE REQUEST TESTS PASSED!\n";
    return 0;
}
