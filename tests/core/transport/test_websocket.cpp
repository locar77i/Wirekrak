/*
================================================================================
WebSocket Transport Unit Tests
================================================================================

These tests validate the correctness of Wirekrak’s WebSocket transport layer
*without* relying on WinHTTP, the OS, or real network I/O.

Key design goals demonstrated here:
  • Transport / policy separation — only transport invariants are tested
  • Deterministic behavior — no network, no timing dependencies
  • Exactly-once failure signaling — close callbacks fire once and only once
  • Idempotent shutdown semantics — safe repeated close() calls
  • Testability by design — WinHTTP is injected as a compile-time policy

The WebSocket is exercised through the real implementation
(WebSocketImpl<ApiConcept>), while a fake WinHTTP backend is used to simulate
errors, close frames, and message delivery.

This approach mirrors production-grade trading SDKs, where transport logic is
unit-tested independently from OS and network behavior, ensuring fast, reliable,
and CI-safe tests.

All transport tests are designed to pass identically in Debug and Release,
avoiding timing assumptions and relying only on observable transport invariants.
================================================================================
*/

#include <cassert>
#include <atomic>
#include <queue>
#include <thread>
#include <iostream>
#include <cstring>
#include <algorithm>

#include "wirekrak/core/transport/websocket_concept.hpp"
#include "wirekrak/core/transport/websocket.hpp"
#include "wirekrak/core/policy/transport/websocket_bundle.hpp"
#include "wirekrak/core/preset/control_ring_default.hpp"
#include "wirekrak/core/preset/message_ring_default.hpp"
#include "lcr/memory/block_pool.hpp"
#include "common/test_check.hpp"


namespace wirekrak::core::transport {
namespace test {

using transport::websocket::ReceiveStatus;

struct TestBackend {
    std::queue<websocket::ReadResult> results;
    std::queue<std::string> payloads;

    int read_count = 0;
    int send_count = 0;
    int close_count = 0;

    bool open = true;
    bool send_result = true;

    // --- Lifecycle ---
    bool connect(std::string_view, std::uint16_t, std::string_view, bool) noexcept {
        open = true;
        return true;
    }

    void close() noexcept {
        open = false;
        ++close_count;
    }

    bool is_open() const noexcept {
        return open;
    }

    // --- Send ---
    bool send(std::string_view) noexcept {
        ++send_count;
        return send_result;
    }

    // --- Receive (NEW CONTRACT) ---
    websocket::ReadResult read_some(void* buffer, std::size_t size) noexcept {
        ++read_count;

        if (results.empty()) {
            std::this_thread::yield();
            return {
                .status = websocket::ReceiveStatus::Ok,
                .bytes  = 0,
                .frame  = websocket::FrameType::Close
            };
        }

        auto result = results.front();
        results.pop();

        // Enforce contract in test backend
        if (result.status != websocket::ReceiveStatus::Ok) {
            return result; // bytes must already be 0
        }

        // Write payload if present
        if (result.status == websocket::ReceiveStatus::Ok && result.bytes > 0) {
            if (payloads.empty()) [[unlikely]] {
                // Test bug → fail hard
                std::abort(); // or assert(false)
            }

            const auto& p = payloads.front();
            auto n = std::min(p.size(), size);

            std::memcpy(buffer, p.data(), n);
            result.bytes = n;

            payloads.pop();
        }

        return result;
    }
};

} // namespace test

static_assert(websocket::BackendConcept<test::TestBackend>);

} // namespace wirekrak::core::transport


// -----------------------------------------------------------------------------
// Setup environment
// -----------------------------------------------------------------------------
using namespace wirekrak::core;
using namespace wirekrak::core::transport;

using ControlRingUnderTest = preset::DefaultControlRing; // Golbal control ring buffer (transport → session)
using MessageRingUnderTest = preset::DefaultMessageRing; // Golbal message ring buffer (transport → session)


using WebSocketUnderTest =
    WebSocketImpl<
        ControlRingUnderTest,
        MessageRingUnderTest,
        policy::transport::DefaultWebsocket,
        test::TestBackend
    >;

// Assert that WebSocketUnderTest conforms to transport::WebSocketConcept concept
static_assert(WebSocketConcept<WebSocketUnderTest>);

// -------------------------------------------------------------------------
// Golbal control SPSC ring buffer (transport → session)
// -----------------------------------------------------------------------------
static ControlRingUnderTest control_ring;

// -------------------------------------------------------------------------
// Global memory block pool
// -------------------------------------------------------------------------
inline constexpr static std::size_t BLOCK_SIZE = 128 * 1024; // 128 KiB
inline constexpr static std::size_t BLOCK_COUNT = 8;
static lcr::memory::block_pool memory_pool(BLOCK_SIZE, BLOCK_COUNT);

// -----------------------------------------------------------------------------
// Golbal SPSC ring buffer (transport → session)
// -----------------------------------------------------------------------------
static MessageRingUnderTest message_ring(memory_pool);


// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

void test_close_called_once() {
    std::cout << "[TEST] Running close() called once test..." << std::endl;

    // Reset global rings before test
    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(control_ring, message_ring, telemetry);

    // Flag to detect when receive loop has started
    std::atomic<bool> receive_started{false};
    ws.set_receive_started_flag(&receive_started);

    // --- Simulate CLOSE from backend ---
    auto& backend = ws.test_backend();
    backend.results.push({
        .status = websocket::ReceiveStatus::Ok,
        .bytes  = 0,
        .frame  = websocket::FrameType::Close
    });

    // Start receive loop (no real connect needed) - ws.connect("x", 443, "/", true))
    ws.test_start_receive_loop();

    // Wait for receive loop to start (better synchronization than sleep)
    while (!receive_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    ws.close();
    ws.close(); // idempotent

    // Drain control-plane events
    int close_count{0};

    websocket::Event ev;
    while (ws.poll_event(ev)) {
        switch (ev.type) {
        case websocket::EventType::Close:
            close_count++;
            break;

        default:
            break;
        }
    }

    assert(close_count == 1);
    std::cout << "[TEST] Done." << std::endl;
}


void test_error_triggers_close() {
    std::cout << "[TEST] Running error triggers close test..." << std::endl;

    // Reset global rings before test
    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(control_ring, message_ring, telemetry);

    std::atomic<bool> receive_started{false};
    ws.set_receive_started_flag(&receive_started);

    // --- Simulate transport error ---
    auto& backend = ws.test_backend();
    backend.results.push({
        .status = websocket::ReceiveStatus::TransportError,
        .bytes  = 0,
        .frame  = websocket::FrameType::Fragment // ignored
    });

    // Start receive loop
    ws.test_start_receive_loop();

    // Wait until receive loop is active
    while (!receive_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // Ensure backend was actually polled at least once
    while (backend.read_count == 0) {
        std::this_thread::yield();
    }

    ws.close();

    // Drain control-plane events
    int error_count = 0;
    int close_count = 0;
    Error last_error = Error::None;

    websocket::Event ev;
    while (ws.poll_event(ev)) {
        switch (ev.type) {
        case websocket::EventType::Error:
            error_count++;
            last_error = ev.error;
            break;

        case websocket::EventType::Close:
            close_count++;
            break;

        default:
            break;
        }
    }

    // --- Assertions (transport invariants) ---

    // At most one error (failure-first model)
    assert(error_count <= 1);

    // Close must ALWAYS happen
    assert(close_count == 1);

    // If error emitted → must be TransportFailure
    if (error_count == 1) {
        assert(last_error == Error::TransportFailure);
    }

    std::cout << "[TEST] Done." << std::endl;
}


void test_message_delivery_to_ring() {
    std::cout << "[TEST] Running message delivery to ring test..." << std::endl;

    // Reset global rings before test
    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(control_ring, message_ring, telemetry);

    std::atomic<bool> receive_started{false};
    ws.set_receive_started_flag(&receive_started);

    // --- Simulate one full message ---
    auto& backend = ws.test_backend();

    backend.payloads.push("test_message");
    backend.results.push({
        .status = websocket::ReceiveStatus::Ok,
        .bytes  = 12, // or any >0 (will be adjusted)
        .frame  = websocket::FrameType::Message
    });

    // Start receive loop
    ws.test_start_receive_loop();

    // Wait until receive loop is active
    while (!receive_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // Wait until backend was actually read
    while (backend.read_count < 1) {
        std::this_thread::yield();
    }

    // Now message must be available
    auto* slot = ws.peek_message();
    while (slot == nullptr) {
        std::this_thread::yield();
        slot = ws.peek_message();
    }

    assert(slot != nullptr);

    // Assert payload
    const char* expected = "test_message";
    const std::size_t expected_size = std::strlen(expected);

    assert(slot->size() == expected_size);
    assert(std::memcmp(slot->data(), expected, expected_size) == 0);

    // Release slot (mandatory)
    ws.release_message(slot);

    ws.close();

    // Ensure backend was used
    assert(backend.read_count >= 1);

    std::cout << "[TEST] Done." << std::endl;
}


void test_send_success() {
    std::cout << "[TEST] Running send success test..." << std::endl;

    // Reset global rings before test
    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(control_ring, message_ring, telemetry);

    // Simulate open connection
    auto& backend = ws.test_backend();
    backend.open = true;

    // NOTE: send() is synchronous and independent from receive loop
    bool ok = ws.send("hello");

    assert(ok);
    assert(backend.send_count == 1);

    std::cout << "[TEST] Done." << std::endl;
}


void test_send_failure() {
    std::cout << "[TEST] Running send failure test..." << std::endl;

    // Reset global rings before test
    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(control_ring, message_ring, telemetry);

    // Simulate open connection but failing send
    auto& backend = ws.test_backend();
    backend.open = true;
    backend.send_result = false;

    // send() is synchronous and independent from receive loop
    bool ok = ws.send("hello");

    assert(!ok);
    assert(backend.send_count == 1);

    std::cout << "[TEST] Done." << std::endl;
}


void test_error_then_close_ordering() {
    std::cout << "[TEST] Running error -> close ordering test..." << std::endl;

    // Reset global rings before test
    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(control_ring, message_ring, telemetry);

    std::atomic<bool> receive_started{false};
    ws.set_receive_started_flag(&receive_started);

    // --- Simulate transport error ---
    auto& backend = ws.test_backend();
    backend.results.push({
        .status = websocket::ReceiveStatus::TransportError,
        .bytes  = 0,
        .frame  = websocket::FrameType::Fragment // ignored
    });

    // Start receive loop
    ws.test_start_receive_loop();

    // Wait until receive loop has actually started
    while (!receive_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // Wait until backend was actually read
    while (backend.read_count == 0) {
        std::this_thread::yield();
    }

    ws.close();

    // Drain control-plane events
    std::vector<std::string> events;
    Error last_error = Error::None;

    websocket::Event ev;
    while (ws.poll_event(ev)) {
        switch (ev.type) {
        case websocket::EventType::Error:
            events.push_back("error");
            last_error = ev.error;
            break;

        case websocket::EventType::Close:
            events.push_back("close");
            break;

        default:
            break;
        }
    }

    std::cout << "Observed events in order: " << events.size() << " events\n";
    for (const auto& e : events) {
        std::cout << "  " << e << "\n";
    }

    // --- Assertions (strict ordering contract) ---

    assert(events.size() == 2);
    assert(events[0] == "error");
    assert(events[1] == "close");

    // Transport-level mapping (normalized)
    assert(last_error == Error::TransportFailure);

    std::cout << "[TEST] Done." << std::endl;
}


void test_multiple_messages() {
    std::cout << "[TEST] Running multiple message test..." << std::endl;

    // Reset global rings before test
    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(control_ring, message_ring, telemetry);

    std::atomic<bool> receive_started{false};
    ws.set_receive_started_flag(&receive_started);

    // --- Simulate two full messages + close ---
    auto& backend = ws.test_backend();

    backend.payloads.push("msg1");
    backend.results.push({
        .status = websocket::ReceiveStatus::Ok,
        .bytes  = 4,
        .frame  = websocket::FrameType::Message
    });

    backend.payloads.push("msg2");
    backend.results.push({
        .status = websocket::ReceiveStatus::Ok,
        .bytes  = 4,
        .frame  = websocket::FrameType::Message
    });

    backend.results.push({
        .status = websocket::ReceiveStatus::Ok,
        .bytes  = 0,
        .frame  = websocket::FrameType::Close
    });

    // Start receive loop
    ws.test_start_receive_loop();

    // Wait until receive loop is active
    while (!receive_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // Wait until both messages have been processed
    while (backend.read_count < 2) {
        std::this_thread::yield();
    }

    int count = 0;
    auto* slot = ws.peek_message();

    while (slot != nullptr) {
        if (count == 0) {
            assert(slot->size() == 4);
            assert(std::memcmp(slot->data(), "msg1", 4) == 0);
        }
        else if (count == 1) {
            assert(slot->size() == 4);
            assert(std::memcmp(slot->data(), "msg2", 4) == 0);
        }

        std::cout << " -> Message " << count + 1
                  << ": " << std::string(slot->data(), slot->size()) << "\n";

        ws.release_message(slot);
        count++;

        slot = ws.peek_message();
    }

    ws.close();

    std::cout << "Total messages received: " << count << std::endl;

    assert(count == 2);

    std::cout << "[TEST] Done." << std::endl;
}


void test_fragment_assembly() {
    std::cout << "[TEST] Running fragment assembly test..." << std::endl;

    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(control_ring, message_ring, telemetry);

    std::atomic<bool> receive_started{false};
    ws.set_receive_started_flag(&receive_started);

    auto& backend = ws.test_backend();

    backend.payloads.push("hello ");
    backend.results.push({
        .status = websocket::ReceiveStatus::Ok,
        .bytes  = 6,
        .frame  = websocket::FrameType::Fragment
    });

    backend.payloads.push("world");
    backend.results.push({
        .status = websocket::ReceiveStatus::Ok,
        .bytes  = 5,
        .frame  = websocket::FrameType::Message
    });

    ws.test_start_receive_loop();

    while (!receive_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    auto* slot = ws.peek_message();
    while (!slot) {
        std::this_thread::yield();
        slot = ws.peek_message();
    }

    assert(slot->size() == 11);
    assert(std::memcmp(slot->data(), "hello world", 11) == 0);

    ws.release_message(slot);
    ws.close();

    std::cout << "[TEST] Done." << std::endl;
}


void test_invalid_fragment_zero_bytes() {
    std::cout << "[TEST] Running invalid fragment test..." << std::endl;

    control_ring.clear();
    message_ring.clear();

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(control_ring, message_ring, telemetry);

    auto& backend = ws.test_backend();

    backend.results.push({
        .status = websocket::ReceiveStatus::Ok,
        .bytes  = 0, // INVALID
        .frame  = websocket::FrameType::Fragment
    });

    ws.test_start_receive_loop();

    // Expect assert / crash in debug
}


// -----------------------------------------------------------------------------
// Entry point
// -----------------------------------------------------------------------------

int main() {   
    // The WebSocket transport is fully unit-tested for message delivery,
    // error handling, close semantics, callback ordering, idempotent shutdown
    // and send behavior.
    // Tests are deterministic, OS-independent, and exercise the real transport
    // implementation via a compile-time injected WinHTTP API.
    test_close_called_once();
    test_error_triggers_close();
    test_message_delivery_to_ring();
    test_send_success();
    test_send_failure();
    test_error_then_close_ordering();
    test_multiple_messages();
    test_fragment_assembly();
    test_invalid_fragment_zero_bytes();

    std::cout << "[TEST] ALL TRANSPORT TESTS PASSED!" << std::endl;
    return 0;
}
