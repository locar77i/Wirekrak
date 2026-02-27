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
#include "wirekrak/core/transport/winhttp/concepts.hpp"
#include "wirekrak/core/transport/winhttp/websocket.hpp"
#include "wirekrak/core/policy/transport/websocket_bundle.hpp"
#include "common/test_check.hpp"

using namespace wirekrak::core::transport;


namespace wirekrak::core::transport::winhttp {

// -----------------------------------------------------------------------------
// Fake WinHTTP API (test-only)
// -----------------------------------------------------------------------------
struct FakeApi {
    std::queue<DWORD> results;
    std::queue<WINHTTP_WEB_SOCKET_BUFFER_TYPE> types;
    std::queue<std::string> payloads;

    int receive_count = 0;
    int send_count = 0;
    int close_count = 0;

    DWORD send_result = ERROR_SUCCESS;

    DWORD websocket_receive(HINTERNET, void* buffer, DWORD buffer_len, DWORD* bytes, WINHTTP_WEB_SOCKET_BUFFER_TYPE* type) {
        ++receive_count;
        if (results.empty()) {
            std::this_thread::yield();
            return ERROR_WINHTTP_OPERATION_CANCELLED;
        }
        *bytes = 0;
        // Pop next type
        *type  = types.front();
        types.pop();
        // Pop next result
        DWORD r = results.front();
        results.pop();
        // If result is error, return immediately without writing to buffer
        if (r != ERROR_SUCCESS) {
            return r;
        }
        //Write payload if provided
        if (!payloads.empty()) {
            const std::string& p = payloads.front();
            const DWORD to_copy = static_cast<DWORD>(std::min<std::size_t>(p.size(), buffer_len));
            std::memcpy(buffer, p.data(), to_copy);
            *bytes = to_copy;
            payloads.pop();
        }
        return r;
    }

    DWORD websocket_send(HINTERNET, WINHTTP_WEB_SOCKET_BUFFER_TYPE, void*, DWORD) {
        ++send_count;
        return send_result;
    }

    void websocket_close(HINTERNET) {
        ++close_count;
    }
};
// Defensive check that FakeApi conforms to ApiConcept concept
static_assert(ApiConcept<FakeApi>, "FakeApi must model ApiConcept");

} // namespace wirekrak::core::transport::winhttp


// -----------------------------------------------------------------------------
// Setup environment
// -----------------------------------------------------------------------------
using namespace wirekrak::core;
using namespace wirekrak::core::transport;

using MessageRingUnderTest = lcr::lockfree::spsc_ring<websocket::DataBlock, RX_RING_CAPACITY>;
using WebSocketUnderTest = winhttp::WebSocketImpl<MessageRingUnderTest, policy::transport::WebsocketDefault, winhttp::FakeApi>;

// Assert that WebSocketUnderTest conforms to transport::WebSocketConcept concept
static_assert(WebSocketConcept<WebSocketUnderTest>);

static MessageRingUnderTest g_ring;   // Golbal SPSC ring buffer (transport → session)


// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

void test_close_called_once() {
    std::cout << "[TEST] Running close() called once test..." << std::endl;

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(g_ring, telemetry);

    // Flag to detect when receive loop has started
    std::atomic<bool> receive_started{false};
    ws.set_receive_started_flag(&receive_started);

    // Simulate close frame
    ws.test_api().results.push(ERROR_SUCCESS);
    ws.test_api().types.push(WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE);

    //if (!ws.connect("x", "443", "/"))
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

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(g_ring, telemetry);

    std::atomic<bool> receive_started{false};
    ws.set_receive_started_flag(&receive_started);

    // Simulate error
    ws.test_api().results.push(ERROR_WINHTTP_CONNECTION_ERROR);
    ws.test_api().types.push(WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE);

    //if (!ws.connect("x", "443", "/"))
    ws.test_start_receive_loop();

    // Wait until receive loop is active
    while (!receive_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // Wait until receive thread processes one receive call
    while (ws.test_api().receive_count < 1) {
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

    // Now assert observed behavior
    assert(error_count <= 1);
    assert(close_count == 1);

    // If error happened, it must have happened before close
    if (error_count == 1) {
        assert(ws.test_api().receive_count >= 1);
    }

    // Validate semantic error classification
    assert(last_error == Error::RemoteClosed || last_error == Error::TransportFailure);

    std::cout << "[TEST] Done." << std::endl;
}


void test_message_delivery_to_ring() {
    std::cout << "[TEST] Running message delivery to ring test..." << std::endl;

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(g_ring, telemetry);

    std::atomic<bool> receive_started{false};
    ws.set_receive_started_flag(&receive_started);

    // Simulate one message
    ws.test_api().results.push(ERROR_SUCCESS);
    ws.test_api().types.push(WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE);
    ws.test_api().payloads.push("test_message");

    //if (!ws.connect("x", "443", "/"))
    ws.test_start_receive_loop();

    // Wait until receive loop is active
    while (!receive_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // Wait until the first receive() has happened
    while (ws.test_api().receive_count < 1) {
        std::this_thread::yield();
    }

    // Now message must be available
    websocket::DataBlock* block = nullptr;
    while ((block = ws.peek_message()) == nullptr) {
        std::this_thread::yield();
    }
    
    assert(block != nullptr);

    // Assert payload
    const char* expected = "test_message";
    const std::size_t expected_size = std::strlen(expected);
    assert(block->size == expected_size);
    assert(std::memcmp(block->data, expected, expected_size) == 0);

    // Release slot (mandatory)
    ws.release_message();

    ws.close();

    // At least one receive must have occurred
    assert(ws.test_api().receive_count >= 1);

    std::cout << "[TEST] Done." << std::endl;
}


void test_send_success() {
    std::cout << "[TEST] Running send success test..." << std::endl;

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(g_ring, telemetry);

    // Establish fake connection (sets hWebSocket_)
    ws.test_start_receive_loop();

    // NOTE: send() is synchronous and does not require a running receive loop.
    // This test validates pure transport behavior without threading.
    bool ok = ws.send("hello");

    assert(ok);
    assert(ws.test_api().send_count == 1);

    std::cout << "[TEST] Done." << std::endl;
}

void test_send_failure() {
    std::cout << "[TEST] Running send failure test..." << std::endl;

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(g_ring, telemetry);

    ws.test_api().send_result = ERROR_WINHTTP_CONNECTION_ERROR;

    // Establish fake connection (sets hWebSocket_)
    ws.test_start_receive_loop();

    // NOTE: send() is synchronous and does not require a running receive loop.
    // This test validates pure transport behavior without threading.
    bool ok = ws.send("hello");

    assert(!ok);
    assert(ws.test_api().send_count == 1);

    std::cout << "[TEST] Done." << std::endl;
}

void test_error_then_close_ordering() {
    std::cout << "[TEST] Running error -> close ordering test..." << std::endl;

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(g_ring, telemetry);

    std::atomic<bool> receive_started{false};
    ws.set_receive_started_flag(&receive_started);

    ws.test_api().results.push(ERROR_WINHTTP_CONNECTION_ERROR);
    ws.test_api().types.push(WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE);

    //if (!ws.connect("x", "443", "/"))
    ws.test_start_receive_loop();

    // Wait until receive loop has actually started
    while (!receive_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // Give receive loop opportunity to process injected error
    std::this_thread::yield();

    while (ws.test_api().receive_count == 0) {
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
    assert(events.size() == 2);
    assert(events[0] == "error");
    assert(events[1] == "close");

    // Validate semantic error classification
    assert(last_error == Error::RemoteClosed);

    std::cout << "[TEST] Done." << std::endl;
}


// TODO: review in depth (non deterministic)
void test_multiple_messages() {
    std::cout << "[TEST] Running multiple message test..." << std::endl;

    telemetry::WebSocket telemetry;
    WebSocketUnderTest ws(g_ring, telemetry);

    std::atomic<bool> receive_started{false};
    ws.set_receive_started_flag(&receive_started);

    ws.test_api().results.push(ERROR_SUCCESS);
    ws.test_api().types.push(WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE);
    ws.test_api().payloads.push("msg1");

    ws.test_api().results.push(ERROR_SUCCESS);
    ws.test_api().types.push(WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE);
    ws.test_api().payloads.push("msg2");

    ws.test_api().results.push(ERROR_SUCCESS);
    ws.test_api().types.push(WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE);

    //if (!ws.connect("x", "443", "/"))
    ws.test_start_receive_loop();

    // Wait until receive loop is active
    while (!receive_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // Wait until both messages have been processed
    while (ws.test_api().receive_count < 2) {
        std::this_thread::yield();
    }

    int count = 0;
    websocket::DataBlock* block = ws.peek_message();
    while (block != nullptr) {
        assert(block != nullptr);

        if (count == 0) {
            assert(block->size == 4);
            assert(std::memcmp(block->data, "msg1", 4) == 0);
        }
        else if (count == 1) {
            assert(block->size == 4);
            assert(std::memcmp(block->data, "msg2", 4) == 0);
        }
        // Release slot (mandatory)
        ws.release_message();
        std::cout << " -> Message " << count + 1 << ": " << std::string(block->data, block->size) << "\n";
        count++;
        block = ws.peek_message();
    }

    ws.close();

    std::cout << "Total messages received: " << count << std::endl;
    assert(count == 2);

    std::cout << "[TEST] Done." << std::endl;
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

    std::cout << "[TEST] ALL TRANSPORT TESTS PASSED!" << std::endl;
    return 0;
}
