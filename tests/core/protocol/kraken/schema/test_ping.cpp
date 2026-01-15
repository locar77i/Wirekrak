#include <cassert>
#include <iostream>
#include <string>

#include "wirekrak/core/protocol/kraken/schema/system/ping.hpp"

using namespace wirekrak::core;
using namespace wirekrak::core::protocol::kraken;

/*
================================================================================
Kraken Ping Request — Unit Tests
================================================================================

These tests validate JSON serialization for the Kraken WebSocket
"ping" request.

Design goals enforced by this test suite:
  • Deterministic JSON output
  • Strict schema compliance
  • No implicit defaults leaked into payload
  • Optional fields included only when explicitly set
  • Safe behavior under minimal and full configurations

This test suite validates request construction only. Transport-level
and server-side validation are intentionally out of scope.
================================================================================
*/

void test_ping_minimal() {
    std::cout << "[TEST] Ping request (minimal)..." << std::endl;

    schema::system::Ping ping;

    std::string json = ping.to_json();

    // Required structure
    assert(json == "{\"method\":\"ping\"}");

    // Optional fields must NOT appear
    assert(json.find("\"req_id\"") == std::string::npos);

    std::cout << "[TEST] OK\n";
}

void test_ping_with_req_id() {
    std::cout << "[TEST] Ping request (with req_id)..." << std::endl;

    schema::system::Ping ping;
    ping.req_id = 42;

    std::string json = ping.to_json();

    // Required structure
    assert(json.find("\"method\":\"ping\"") != std::string::npos);

    // Optional field must appear
    assert(json.find("\"req_id\":42") != std::string::npos);

    std::cout << "[TEST] OK\n";
}

void test_ping_large_req_id() {
    std::cout << "[TEST] Ping request (large req_id)..." << std::endl;

    schema::system::Ping ping;
    ping.req_id = 9'223'372'036'854'775'807ULL;

    std::string json = ping.to_json();

    assert(json.find("\"req_id\":9223372036854775807") != std::string::npos);

    std::cout << "[TEST] OK\n";
}

void test_ping_json_is_compact() {
    std::cout << "[TEST] Ping request (compact JSON)..." << std::endl;

    schema::system::Ping ping;
    ping.req_id = 1;

    std::string json = ping.to_json();

    // No whitespace or formatting noise
    assert(json.find(' ') == std::string::npos);
    assert(json.find('\n') == std::string::npos);
    assert(json.find('\t') == std::string::npos);

    std::cout << "[TEST] OK\n";
}

#ifndef NDEBUG
void test_ping_invalid_req_id_asserts() {
    std::cout << "[TEST] Ping request (invalid req_id — debug assert)..." << std::endl;

    schema::system::Ping ping;
    ping.req_id = 0; // invalid by contract

    // NOTE:
    // assert() cannot be reliably caught.
    // This test documents the contract: req_id must be non-zero.
    (void)ping.to_json();

    std::cout << "[TEST] OK (assert expected in debug)\n";
}
#endif

int main() {
    test_ping_minimal();
    test_ping_with_req_id();
    test_ping_large_req_id();
    test_ping_json_is_compact();

#ifndef NDEBUG
    //test_ping_invalid_req_id_asserts();
#endif

    std::cout << "[TEST] ALL PING REQUEST TESTS PASSED!\n";
    return 0;
}
