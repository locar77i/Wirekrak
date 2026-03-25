/*
================================================================================
Backend Contract Compliance Tests
================================================================================

Purpose
-------
This suite validates that any backend implementation satisfies the strict
BackendConcept contract required by the WebSocket transport layer.

Unlike transport tests, these tests intentionally simulate INVALID backend
behavior to ensure that:

  • Contract violations are detected immediately
  • Transport fails fast (asserts in debug)
  • No undefined behavior propagates into the system

Test Philosophy
---------------
These tests are NOT expected to "pass" in the traditional sense.

Instead:
  ✔ Valid cases must pass
  ✔ Invalid cases must trigger assertions (debug) or fail-fast behavior

This ensures that any backend plugged into the system is safe and compliant.

Contract Invariants Tested
--------------------------
1. status != Ok  → bytes MUST be 0
2. Fragment      → bytes MUST be > 0
3. Message       → bytes MAY be >= 0
4. Close         → bytes MUST be 0

================================================================================
*/

#include <cassert>
#include <queue>
#include <cstring>
#include <iostream>
#include <atomic>
#include <thread>

#include "wirekrak/core/transport/websocket.hpp"
#include "wirekrak/core/transport/websocket_concept.hpp"
#include "wirekrak/core/preset/control_ring_default.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"
#include "wirekrak/core/policy/transport/websocket_bundle.hpp"
#include "lcr/memory/block_pool.hpp"

namespace wirekrak::core::transport::test {

// -----------------------------------------------------------------------------
// Strict Test Backend (NO SAFETY NETS)
// -----------------------------------------------------------------------------
struct TestBackend {
    std::queue<websocket::ReadResult> results;
    std::queue<std::string> payloads;

    bool open = true;

    bool connect(std::string_view, std::uint16_t, std::string_view, bool) noexcept {
        return true;
    }

    void close() noexcept {
        open = false;
    }

    bool is_open() const noexcept {
        return open;
    }

    bool send(std::string_view) noexcept {
        return true;
    }

    websocket::ReadResult read_some(void* buffer, std::size_t size) noexcept {
        if (results.empty()) {
            std::abort(); // unexpected read → strict mode
        }

        auto result = results.front();
        results.pop();

        if (result.status == websocket::ReceiveStatus::Ok && result.bytes > 0) {
            if (payloads.empty()) {
                std::abort(); // contract violation in test setup
            }

            const auto& p = payloads.front();
            auto n = std::min(p.size(), size);

            std::memcpy(buffer, p.data(), n);
            payloads.pop();

            result.bytes = n;
        }

        return result;
    }
};

} // namespace wirekrak::core::transport::test

using namespace wirekrak::core;
using namespace wirekrak::core::transport;

using ControlRing = preset::DefaultControlRing;
using MessageRing = preset::DefaultMessageRing;

using WS = WebSocketImpl<
    ControlRing,
    MessageRing,
    policy::transport::DefaultWebsocket,
    test::TestBackend
>;

static ControlRing control_ring;
static lcr::memory::block_pool memory_pool(128 * 1024, 8);
static MessageRing message_ring(memory_pool);

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

void wait_for_loop_start(WS& ws, std::atomic<bool>& flag) {
    ws.set_receive_started_flag(&flag);
    ws.test_start_receive_loop();

    while (!flag.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
}

// -----------------------------------------------------------------------------
// VALID CASES (must pass)
// -----------------------------------------------------------------------------

void test_valid_fragment() {
    std::cout << "[TEST] valid fragment..." << std::endl;

    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WS ws(control_ring, message_ring, telemetry);

    std::atomic<bool> started{false};

    auto& b = ws.test_backend();
    b.payloads.push("abc");

    b.results.push({
        .status = websocket::ReceiveStatus::Ok,
        .bytes  = 3,
        .frame  = websocket::FrameType::Fragment
    });

    wait_for_loop_start(ws, started);

    ws.close();
}

void test_invalid_message_zero_bytes() {
    std::cout << "[TEST] invalid empty message..." << std::endl;

    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WS ws(control_ring, message_ring, telemetry);

    std::atomic<bool> started{false};

    auto& b = ws.test_backend();

    b.results.push({
        .status = websocket::ReceiveStatus::Ok,
        .bytes  = 0,
        .frame  = websocket::FrameType::Message
    });

    wait_for_loop_start(ws, started);

    ws.close();
}

void test_valid_close() {
    std::cout << "[TEST] valid close..." << std::endl;

    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WS ws(control_ring, message_ring, telemetry);

    std::atomic<bool> started{false};

    auto& b = ws.test_backend();

    b.results.push({
        .status = websocket::ReceiveStatus::Ok,
        .bytes  = 0,
        .frame  = websocket::FrameType::Close
    });

    wait_for_loop_start(ws, started);
}

// -----------------------------------------------------------------------------
// INVALID CASES (must assert / fail-fast in debug)
// -----------------------------------------------------------------------------

void test_invalid_fragment_zero_bytes() {
    std::cout << "[TEST] INVALID: fragment with 0 bytes (should assert)" << std::endl;

    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WS ws(control_ring, message_ring, telemetry);

    auto& b = ws.test_backend();

    b.results.push({
        .status = websocket::ReceiveStatus::Ok,
        .bytes  = 0, // ❌ INVALID
        .frame  = websocket::FrameType::Fragment
    });

    ws.test_start_receive_loop();
}

void test_invalid_close_with_bytes() {
    std::cout << "[TEST] INVALID: close with bytes (should assert)" << std::endl;

    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WS ws(control_ring, message_ring, telemetry);

    auto& b = ws.test_backend();

    b.payloads.push("oops");

    b.results.push({
        .status = websocket::ReceiveStatus::Ok,
        .bytes  = 4, // ❌ INVALID
        .frame  = websocket::FrameType::Close
    });

    ws.test_start_receive_loop();
}

void test_invalid_error_with_bytes() {
    std::cout << "[TEST] INVALID: error with bytes (should assert)" << std::endl;

    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WS ws(control_ring, message_ring, telemetry);

    auto& b = ws.test_backend();

    b.results.push({
        .status = websocket::ReceiveStatus::TransportError,
        .bytes  = 5, // ❌ INVALID
        .frame  = websocket::FrameType::Fragment
    });

    ws.test_start_receive_loop();
}

// -----------------------------------------------------------------------------
// Entry point
// -----------------------------------------------------------------------------

int main() {
    std::cout << "=== Backend Contract Compliance Tests ===\n";

    // Valid cases (must pass)
    test_valid_fragment();
    test_invalid_message_zero_bytes();
    test_valid_close();

    std::cout << "\n--- The following tests are expected to ASSERT in Debug ---\n";

    // Uncomment one at a time in debug builds
    // test_invalid_fragment_zero_bytes();
    // test_invalid_close_with_bytes();
    // test_invalid_error_with_bytes();

    std::cout << "\n[SUCCESS] Contract validation complete.\n";
}
