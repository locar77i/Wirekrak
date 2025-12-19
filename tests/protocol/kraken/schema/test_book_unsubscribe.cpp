#include <cassert>
#include <iostream>
#include <string_view>

#include "wirekrak/protocol/kraken/book/unsubscribe.hpp"

using namespace wirekrak;
using namespace wirekrak::protocol::kraken;

/*
================================================================================
Kraken Book Unsubscribe Request — Unit Tests
================================================================================

These tests validate JSON serialization for the Kraken WebSocket
"book unsubscribe" request.

Design goals enforced by this test suite:
  • Deterministic JSON output
  • Strict schema compliance
  • No invalid fields (e.g. snapshot) leaked into payload
  • Optional fields included only when explicitly set
  • Safe behavior under minimal and full configurations

This test suite validates request construction only. Transport-level
and server-side validation are intentionally out of scope.
================================================================================
*/

void test_book_unsubscribe_minimal() {
    std::cout << "[TEST] Book unsubscribe request (minimal)..." << std::endl;

    book::Unsubscribe unsub;
    unsub.symbols = { Symbol{"BTC/USD"} };

    std::string json = unsub.to_json();

    // Required structure
    assert(json.find("\"method\":\"unsubscribe\"") != std::string::npos);
    assert(json.find("\"channel\":\"book\"") != std::string::npos);

    // Symbols
    assert(json.find("\"symbol\":[\"BTC/USD\"]") != std::string::npos);

    // Optional fields must NOT appear
    assert(json.find("\"depth\"") == std::string::npos);
    assert(json.find("\"req_id\"") == std::string::npos);
    assert(json.find("\"snapshot\"") == std::string::npos); // not valid for unsubscribe

    std::cout << "[TEST] OK\n";
}

void test_book_unsubscribe_multiple_symbols() {
    std::cout << "[TEST] Book unsubscribe request (multiple symbols)..." << std::endl;

    book::Unsubscribe unsub;
    unsub.symbols = {
        Symbol{"BTC/USD"},
        Symbol{"ETH/USD"},
        Symbol{"MATIC/GBP"}
    };

    std::string json = unsub.to_json();

    assert(json.find("\"symbol\":[\"BTC/USD\",\"ETH/USD\",\"MATIC/GBP\"]")
           != std::string::npos);

    std::cout << "[TEST] OK\n";
}

void test_book_unsubscribe_with_depth_and_req_id() {
    std::cout << "[TEST] Book unsubscribe request (depth + req_id)..." << std::endl;

    book::Unsubscribe unsub;
    unsub.symbols = { Symbol{"BTC/USD"} };
    unsub.depth   = 25;
    unsub.req_id  = 98765;

    std::string json = unsub.to_json();

    assert(json.find("\"depth\":25") != std::string::npos);
    assert(json.find("\"req_id\":98765") != std::string::npos);

    std::cout << "[TEST] OK\n";
}

#ifndef NDEBUG
void test_book_unsubscribe_empty_symbols_asserts() {
    std::cout << "[TEST] Book unsubscribe request (empty symbols — debug assert)..." << std::endl;

    book::Unsubscribe unsub;

    // NOTE:
    // We cannot reliably catch assert(), but this test exists
    // to document and validate the contract in Debug builds.
    (void)unsub.to_json();

    std::cout << "[TEST] OK (assert expected in debug)\n";
}

void test_book_unsubscribe_invalid_depth_asserts() {
    std::cout << "[TEST] Book unsubscribe request (invalid depth — debug assert)..." << std::endl;

    book::Unsubscribe unsub;
    unsub.symbols = { Symbol{"BTC/USD"} };
    unsub.depth   = 42; // invalid Kraken depth

    // NOTE:
    // assert() cannot be reliably caught.
    // This test documents the contract: invalid depth is a programmer error.
    (void)unsub.to_json();

    std::cout << "[TEST] OK (assert expected in debug)\n";
}
#endif

int main() {
    test_book_unsubscribe_minimal();
    test_book_unsubscribe_multiple_symbols();
    test_book_unsubscribe_with_depth_and_req_id();

#ifndef NDEBUG
    //test_book_unsubscribe_empty_symbols_asserts();
    //test_book_unsubscribe_invalid_depth_asserts();
#endif

    std::cout << "[TEST] ALL BOOK UNSUBSCRIBE REQUEST TESTS PASSED!\n";
    return 0;
}
