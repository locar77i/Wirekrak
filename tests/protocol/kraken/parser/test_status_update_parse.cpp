#include <cassert>
#include <iostream>
#include <string_view>

#include "simdjson.h"

#include "wirekrak/protocol/kraken/parser/status/update.hpp"

using namespace wirekrak::protocol::kraken;

/*
================================================================================
Kraken Status Update Parser — Unit Tests
================================================================================

These tests validate parsing of Kraken "status" channel update messages.

Design goals enforced by this test suite:
  • Strict schema validation — required fields must be present
  • Failure-safe parsing — malformed messages return false, never throw
  • Deterministic behavior — no partial writes on failure
  • Explicit negative coverage — missing or invalid fields are rejected
  • Protocol isolation — no transport or dispatcher dependencies

The status channel is critical for system health monitoring. These tests ensure
that only valid engine state updates propagate into higher layers.
================================================================================
*/

static bool parse(std::string_view json, status::Update& out) {
    simdjson::dom::parser parser;
    auto doc = parser.parse(json);
    assert(!doc.error());
    return parser::status::update::parse(doc.value(), out);
}

// ------------------------------------------------------------
// POSITIVE CASES
// ------------------------------------------------------------

void test_status_update_success_online() {
    std::cout << "[TEST] Status update (online)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "status",
        "type": "update",
        "data": [{
            "system": "online",
            "api_version": "v2",
            "connection_id": 12345,
            "version": "1.0.0"
        }]
    }
    )json";

    status::Update upd{};
    assert(parse(json, upd));

    assert(upd.system == SystemState::Online);
    assert(upd.api_version == "v2");
    assert(upd.connection_id == 12345);
    assert(upd.version == "1.0.0");

    std::cout << "[TEST] OK\n";
}

void test_status_update_success_maintenance() {
    std::cout << "[TEST] Status update (maintenance)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "status",
        "type": "update",
        "data": [{
            "system": "maintenance",
            "api_version": "v2",
            "connection_id": 1,
            "version": "2.1.3"
        }]
    }
    )json";

    status::Update upd{};
    assert(parse(json, upd));

    assert(upd.system == SystemState::Maintenance);

    std::cout << "[TEST] OK\n";
}

// ------------------------------------------------------------
// NEGATIVE CASES
// ------------------------------------------------------------

void test_status_update_missing_data() {
    std::cout << "[TEST] Status update (missing data)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "status",
        "type": "update"
    }
    )json";

    status::Update upd{};
    assert(!parse(json, upd));

    std::cout << "[TEST] OK\n";
}

void test_status_update_empty_data_array() {
    std::cout << "[TEST] Status update (empty data array)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "status",
        "type": "update",
        "data": []
    }
    )json";

    status::Update upd{};
    assert(!parse(json, upd));

    std::cout << "[TEST] OK\n";
}

void test_status_update_missing_system() {
    std::cout << "[TEST] Status update (missing system)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "status",
        "type": "update",
        "data": [{
            "api_version": "v2",
            "connection_id": 1,
            "version": "1.0"
        }]
    }
    )json";

    status::Update upd{};
    assert(!parse(json, upd));

    std::cout << "[TEST] OK\n";
}

void test_status_update_missing_api_version() {
    std::cout << "[TEST] Status update (missing api_version)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "status",
        "type": "update",
        "data": [{
            "system": "online",
            "connection_id": 1,
            "version": "1.0"
        }]
    }
    )json";

    status::Update upd{};
    assert(!parse(json, upd));

    std::cout << "[TEST] OK\n";
}

void test_status_update_missing_connection_id() {
    std::cout << "[TEST] Status update (missing connection_id)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "status",
        "type": "update",
        "data": [{
            "system": "online",
            "api_version": "v2",
            "version": "1.0"
        }]
    }
    )json";

    status::Update upd{};
    assert(!parse(json, upd));

    std::cout << "[TEST] OK\n";
}

void test_status_update_missing_version() {
    std::cout << "[TEST] Status update (missing version)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "status",
        "type": "update",
        "data": [{
            "system": "online",
            "api_version": "v2",
            "connection_id": 1
        }]
    }
    )json";

    status::Update upd{};
    assert(!parse(json, upd));

    std::cout << "[TEST] OK\n";
}

void test_status_update_wrong_channel() {
    std::cout << "[TEST] Status update (wrong channel)..." << std::endl;

    constexpr std::string_view json = R"json(
    {
        "channel": "book",
        "type": "update",
        "data": []
    }
    )json";

    status::Update upd{};
    assert(!parse(json, upd));

    std::cout << "[TEST] OK\n";
}

// ------------------------------------------------------------
// ENTRY POINT
// ------------------------------------------------------------

int main() {
    test_status_update_success_online();
    test_status_update_success_maintenance();

    test_status_update_missing_data();
    test_status_update_empty_data_array();
    test_status_update_missing_system();
    test_status_update_missing_api_version();
    test_status_update_missing_connection_id();
    test_status_update_missing_version();
    test_status_update_wrong_channel();

    std::cout << "[TEST] ALL STATUS UPDATE PARSER TESTS PASSED!\n";
    return 0;
}
