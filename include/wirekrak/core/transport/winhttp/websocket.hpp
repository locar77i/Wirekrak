#pragma once

#include <string>
#include <string_view>
#include <thread>
#include <functional>
#include <atomic>
#include <vector>
#include <cassert>

#include "wirekrak/core/transport/error.hpp"
#include "wirekrak/core/transport/winhttp/real_api.hpp"
#include "wirekrak/core/transport/telemetry/websocket.hpp"
#include "wirekrak/core/transport/websocket/events.hpp"
#include "wirekrak/core/transport/websocket/config.hpp"
#include "wirekrak/core/transport/websocket/data_block.hpp"
#include "wirekrak/core/policy/transport/websocket.hpp"
#include "wirekrak/core/telemetry.hpp"
#include "lcr/lockfree/spsc_ring.hpp"
#include "lcr/log/logger.hpp"

#include <windows.h>
#include <winhttp.h>
#include <winerror.h>

/*
================================================================================
WebSocket Transport (WinHTTP minimal implementation)
================================================================================

This header implements the Wirekrak WebSocket transport using WinHTTP, following
a strict separation between *transport mechanics* and *connection policy*.

Design highlights:
  • Single-connection transport primitive — no retries, no reconnection logic
  • Policy-free by design — recovery and subscription replay live in the Client
  • Failure-first signaling — transport errors and close frames are propagated
    immediately and exactly once
  • Deterministic lifecycle — idempotent close() and explicit state transitions
  • Testability by construction — WinHTTP calls are injected as a compile-time
    policy (WebSocketImpl<ApiConcept>), enabling unit tests without OS or network

The templated design allows the same WebSocket implementation to be exercised
against a fake WinHTTP backend in unit tests, while remaining zero-overhead and
fully inlined in production builds.

This approach mirrors production-grade trading SDKs, where transport correctness
is validated independently from the operating system and network stack.
================================================================================
*/


// helper to convert UTF-8 string to wide string
inline std::wstring to_wide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    std::wstring out(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &out[0], size);
    return out;
}


namespace wirekrak::core::transport::winhttp {

template<
    typename MessageRing,
    typename PolicyBundle = policy::transport::WebsocketDefault,
    ApiConcept Api = RealApi
>
class WebSocketImpl {
public:
    explicit WebSocketImpl(MessageRing& ring, telemetry::WebSocket& telemetry) noexcept
        : message_ring_(ring)
        , telemetry_(telemetry) {
    }

    ~WebSocketImpl() {
        close();
        if (hSession_) {
            WinHttpCloseHandle(hSession_);
            hSession_ = nullptr;
        }
    }

    [[nodiscard]]
    inline Error connect(const std::string& host, const std::string& port, const std::string& path) noexcept {
        hSession_ = WinHttpOpen(
            L"Wirekrak/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
        if (!hSession_) {
            WK_ERROR("[WS] WinHttpOpen failed");
            return Error::TransportFailure;
        }

        hConnect_ = WinHttpConnect(hSession_, to_wide(host).c_str(), std::stoi(port), 0);
        if (!hConnect_) {
            WK_ERROR("[WS] WinHttpConnect failed");
            return Error::ConnectionFailed;
        }

        hRequest_ = WinHttpOpenRequest(
            hConnect_,
            L"GET",
            to_wide(path).c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE
        );
        if (!hRequest_) {
            WK_ERROR("[WS] WinHttpOpenRequest failed");
            return Error::TransportFailure;;
        }

        if (!WinHttpSetOption(hRequest_, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) {
            WK_ERROR("[WS] WinHttpSetOption failed");
            return Error::ProtocolError;
        }

        if (!WinHttpSendRequest(hRequest_, nullptr, 0, nullptr, 0, 0, 0)) {
            WK_ERROR("[WS] WinHttpSendRequest failed");
            return Error::HandshakeFailed;
        }

        if (!WinHttpReceiveResponse(hRequest_, nullptr)) {
            WK_ERROR("[WS] WinHttpReceiveResponse failed");
            return Error::HandshakeFailed;
        }

        hWebSocket_ = WinHttpWebSocketCompleteUpgrade(hRequest_, 0);
        if (!hWebSocket_) {
            WK_ERROR("[WS] WinHttpWebSocketCompleteUpgrade failed");
            return Error::HandshakeFailed;
        }

        running_.store(true, std::memory_order_relaxed);
        closed_.store(false, std::memory_order_relaxed);
        recv_thread_ = std::thread(&WebSocketImpl::receive_loop_, this);
        return Error::None;
    }

    // Send a text message. Returns true on success.
    // A boolean “accepted / not accepted” is the honest signal.
    // Errors are reported asynchronously via the error callback.
    [[nodiscard]]
    inline bool send(std::string_view msg) noexcept {
        // 1. Check preconditions (connected state)
        if (!hWebSocket_) [[unlikely]] {
            WK_ERROR("[WS] send() called on unconnected WebSocket");
            return false;
        }
        // 2. Call WebSocket send API
        WK_TRACE("[WS:API] Sending message ... (size " << msg.size() << ")");
        const DWORD result = api_.websocket_send(
            hWebSocket_,
            WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,
            const_cast<char*>(msg.data()),
            static_cast<DWORD>(msg.size())
        );
        // 3. Handle possible errors
        if (result != ERROR_SUCCESS) [[unlikely]] {
            WK_ERROR("[WS] websocket_send() failed");
            return false;
        }
        // 4. Update telemetry
        WK_TL1( telemetry_.bytes_tx_total.inc(msg.size()) );
        WK_TL1( telemetry_.messages_tx_total.inc() );
        return true;
    }

    // Close - No guards and no early return (idempotent)
    inline void close() noexcept {
        // 1. Stop receive loop
        running_.store(false, std::memory_order_release);

        // 2. Cancel blocking receive (if still active)
        if (hWebSocket_) {
            WK_TRACE("[WS:API] Closing WebSocket ...");
            api_.websocket_close(hWebSocket_);
        }

        // 3. Join thread if joinable
        if (recv_thread_.joinable()) {
            recv_thread_.join();
        }

        // 4. Always release handles deterministically
        if (hWebSocket_) { WinHttpCloseHandle(hWebSocket_); hWebSocket_ = nullptr; }
        if (hRequest_)   { WinHttpCloseHandle(hRequest_);   hRequest_ = nullptr; }
        if (hConnect_)   { WinHttpCloseHandle(hConnect_);   hConnect_ = nullptr; }
        //if (hSession_)   { WinHttpCloseHandle(hSession_);   hSession_ = nullptr; }
        WK_TRACE("[WS] WebSocket closed.");
    }

    [[nodiscard]]
    inline bool poll_event(websocket::Event& out) noexcept {
        return control_events_.pop(out);
    }

    [[nodiscard]]
    inline websocket::DataBlock* peek_message() noexcept {
        return message_ring_.peek_consumer_slot();
    }

    inline void release_message() noexcept {
        message_ring_.release_consumer_slot();
    }

    [[nodiscard]]
    inline telemetry::WebSocket& telemetry() noexcept {
        return telemetry_;
    }

private:
    // The receive loop is the heart of the transport's receive path.
    // Key features:
    // - Lock-free
    // - Zero-copy
    // - ULL-safe
    // - Deterministic
    inline void receive_loop_() noexcept {
#ifdef WK_UNIT_TEST
        // Debug builds exposed a race in the test harness.
        // Fixed it by adding a test-only synchronization hook to the transport so
        // tests wait on real transport state instead of timing assumptions.
        if (receive_started_flag_) {
            receive_started_flag_->store(true, std::memory_order_release);
        }
#endif // WK_UNIT_TEST

        uint32_t fragments = 0;
        websocket::DataBlock* current_slot = nullptr;
        std::uint32_t current_size = 0;
        
        // Receive internal loop
        while (running_.load(std::memory_order_acquire)) {
            DWORD bytes = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE type;

            // Acquire slot lazily (only when starting a message)
            if (!current_slot) {
                current_slot = acquire_slot_();
                if (! current_slot) [[unlikely]] {
                    continue; // backpressure handling (no slot available)
                }
                current_size = 0;
            }
            // === Call WebSocket receive API ===
            WK_TRACE("[WS:API] Blocking on WebSocket receive ...");
            DWORD remaining = websocket::RX_BUFFER_SIZE - current_size;
            if (remaining == 0) [[unlikely]] {
                handle_fatal_error_("[WS] Failed to receive incoming message (size exceeds buffer capacity)"
                    " - transport correctness compromised (message will be truncated)", Error::ProtocolError);
                // abandon current slot (not committed)
                current_slot = nullptr;
                current_size = 0;
                break;
            }
            DWORD result = api_.websocket_receive(
                hWebSocket_,
                current_slot->data + current_size,
                remaining,
                &bytes,
                &type
            );
            // === Handle errors ===
            if (result != ERROR_SUCCESS) [[unlikely]] { // abnormal termination
                WK_TL1( telemetry_.receive_errors_total.inc() );
                auto error = handle_receive_error_(result);
                if (!control_events_.push(websocket::Event::make_error(error))) {
                    WK_ERROR("[WS] Failed to emit error <" << to_string(error) << ">  - Event lost in transport shutdown");
                }
                // Stop the loop and signal close
                running_.store(false, std::memory_order_release);
                signal_close_();
                // abandon current slot (not committed)
                current_slot = nullptr;
                current_size = 0;
                break;
            }
            else { // successful receive
                // bytes_rx_total counts raw bytes received from the WebSocket API,
                // including fragments and control frames.
                WK_TL1( telemetry_.bytes_rx_total.inc(bytes) );
            }
            // === Handle close frame ===
            if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) { // normal termination
                WK_DEBUG("[WS] Received WebSocket close frame.");
                // Stop the loop and signal close
                running_.store(false, std::memory_order_release);
                signal_close_();
                // abandon current slot (not committed)
                current_slot = nullptr;
                current_size = 0;
                break;
            }
            current_size += bytes;
            // === Handle final message frame ===
            if (type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)  [[likely]] {
                if (fragments > 0) {
                    WK_TRACE("[WS] Received final message fragment (size " << bytes << ", total message size " << current_size << ", fragments " << fragments << ")");
                    WK_TL1( telemetry_.rx_fragments_total.inc() );
                }
                WK_TL1( telemetry_.rx_message_bytes.set(current_size) );
                WK_TL1( telemetry_.messages_rx_total.inc() );
                WK_TL1( telemetry_.fragments_per_message.record(fragments + 1) );
                // Commit the complete message to the ring buffer
                current_slot->size = current_size;
                message_ring_.commit_producer_slot();
                // Reset for the next message
                fragments = 0;
                current_slot = nullptr;
                current_size = 0;  
            }
            else // === Handle message fragments ===
            if (type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE) {
                WK_TRACE("[WS] Received message fragment (size " << bytes << ")");
                WK_TL1( telemetry_.rx_fragments_total.inc() );
                ++fragments;
            }
        }
        // If we exit loop without explicit close signaling
        signal_close_();
    }

    [[nodiscard]]
    inline websocket::DataBlock* acquire_slot_() noexcept {
        websocket::DataBlock* slot = message_ring_.acquire_producer_slot();
        if (!slot) [[unlikely]] {
            // 1. Get the policy-defined backpressure handling mode
            constexpr auto mode = BackpressurePolicy::on_ring_full();
            // 2. Handle backpressure according to the policy
            if constexpr (mode == core::policy::BackpressureMode::ZeroTolerance) {
                // 2.1. Immediate escalation with forced close
                WK_WARN("[WS] Backpressure: Ring buffer full, applying zero-tolerance backpressure policy (immediate event and forced close)");
                if (!control_events_.push(websocket::Event::make_backpressure())) {
                    WK_FATAL("[WS] Failed to emit backpressure event - control event lost in transport shutdown");
                }
                handle_fatal_error_("[WS] Failed to acquire message slot (backpressure)"
                    " - transport correctness compromised (user is not draining fast enough)", Error::Backpressure);
                return nullptr;
            }
            else
            if constexpr (mode == core::policy::BackpressureMode::Strict) {
                // 2.2. Immediate escalation (session decides fate)
                if (!backpressure_.signaled) {
                    backpressure_.signaled = true;
                    WK_WARN("[WS] Backpressure: Ring buffer full, applying strict backpressure policy (immediate event)");
                    if (!control_events_.push(websocket::Event::make_backpressure())) {
                        handle_fatal_error_("[WS] Failed to emit backpressure event (backpressure)"
                            " - transport correctness compromised (user is not draining fast enough)", Error::Backpressure);
                    }
                }
                std::this_thread::yield();
                return nullptr;
            }
            else
            if constexpr (mode == core::policy::BackpressureMode::Relaxed) {
                // 2.3. Tolerate temporarily before signaling (session decides fate)
                ++backpressure_.attempts;
                if (!backpressure_.signaled && backpressure_.attempts >= backpressure_.threshold) {
                    backpressure_.signaled = true;
                    // Emit backpressure event after reaching threshold
                    WK_WARN("[WS] Backpressure: Ring buffer full, applying relaxed backpressure policy (threshold reached)");
                    if (!control_events_.push(websocket::Event::make_backpressure())) {
                        handle_fatal_error_("[WS] Failed to emit backpressure event (backpressure)"
                            " - transport correctness compromised (user is not draining fast enough)", Error::Backpressure);
                        return nullptr;
                    }
                    backpressure_.attempts = 0;
                }
                // Slow down reader slightly to give it a chance to catch up before retrying 
                if (backpressure_.attempts < 10) { // Light backoff for initial attempts to reduce CPU pressure
                    std::this_thread::yield();
                }
                else { // Stronger backoff after multiple attempts to reduce scheduler dependency
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
                return nullptr;
            }
        }
        // Successfully acquired slot
        backpressure_.reset();
        return slot;
    }

    inline Error handle_receive_error_(DWORD error) noexcept {
        switch (error) {
        case ERROR_WINHTTP_OPERATION_CANCELLED: // ERR_OPERATION_CANCELLED 12017 (local close)
            // Local shutdown, expected during close()
            WK_TRACE("[WS] Receive cancelled (local shutdown)");
            return Error::LocalShutdown;

        case ERROR_WINHTTP_CONNECTION_ERROR: // ERR_CONNECTION_ABORTED 12030 (peer closed)
            // Remote closed connection (no CLOSE frame)
            WK_DEBUG("[WS] Connection closed by peer");
            return Error::RemoteClosed;

        case ERROR_WINHTTP_TIMEOUT: // ERR_TIMED_OUT 12002 (timeout)
            // Network stalled or idle timeout
            WK_WARN("[WS] Receive timeout");
            return Error::Timeout;

        case ERROR_WINHTTP_CANNOT_CONNECT: // ERR_CONNECTION_FAILED 12029 (connect failed)
            // Usually handshake or DNS issues
            WK_ERROR("[WS] Cannot connect to remote host");
            return Error::ConnectionFailed;

        default:
            // Anything else is unexpected
            WK_ERROR("[WS] Receive failed with error code " << error);
            return Error::TransportFailure;
        }
    }

    inline void signal_close_() noexcept {
        // Ensure close callback is invoked exactly once
        if (closed_.exchange(true)) {
            return;
        }
        WK_TL1( telemetry_.close_events_total.inc() );
        if (!control_events_.push(websocket::Event::make_close())) {
            WK_ERROR("[WS] Failed to emit close event (lost in transport shutdown)");
        }
    }

    inline void handle_fatal_error_(const char* message, Error error) noexcept {
        WK_WARN(message);
        // 1. Ensure only one thread performs fatal shutdown
        // Future backpresusre policy (default:strict)
        // Wirekrak should never lie to the user or perform magic without explicit user instruction
        // Defensive action: close the connection to prevent further damage
        if (!running_.exchange(false, std::memory_order_acq_rel)) {
            return; // already shutting down
        }
        WK_FATAL("[WS] Forcing transport close to preserve correctness guarantees.");
        // 2. Emit error event if possible
        if (!control_events_.push(websocket::Event::make_error(error))) {
            WK_ERROR("[WS] Failed to emit error <" << to_string(error) << ">  - Event lost in transport shutdown");
        }
        // 3. Signal close to ensure transport is fully closed (exactly-once guarded)
        signal_close_();
    }

private:
    // Telemetry reference (non-owning) 
    telemetry::WebSocket& telemetry_;

    // Policy-defined backpressure handling (default: strict)
    using BackpressurePolicy = typename PolicyBundle::backpressure;

    // Backpressure state tracking
    struct BackpressureState {
        std::uint32_t attempts{0};
        static constexpr std::uint32_t threshold{50};
        bool signaled{false};
        // Resets the backpressure state (e.g. after successfully acquiring a slot)
        void reset() noexcept {
            attempts = 0;
            signaled = false;
        }
    };
    BackpressureState backpressure_;

    // Compile-time API policy (default: RealApi)
    Api api_;

    // WinHTTP handles
    HINTERNET hSession_   = nullptr;
    HINTERNET hConnect_   = nullptr;
    HINTERNET hRequest_   = nullptr;
    HINTERNET hWebSocket_ = nullptr;

    std::thread recv_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> closed_{false};

    // Control event queue (for signaling events like close and error)
    lcr::lockfree::spsc_ring<websocket::Event, CTRL_RING_CAPACITY> control_events_;

    // Data message queue (transport → connection/session)
    MessageRing& message_ring_;

#ifdef WK_UNIT_TEST
public:
    // Test-only accessor to the internal API
    Api& test_api() noexcept {
        return api_;
    }

public:
    // Test-only method to start receive loop without connect()
    void test_start_receive_loop() noexcept {
        WK_TRACE("[WS:TEST] Connecting WebSocket (simulated) ...");
        assert(!test_receive_loop_started_ && "test_start_receive_loop() called twice");
        test_receive_loop_started_ = true;
        // Fake non-null WebSocket handle
        hWebSocket_ = reinterpret_cast<HINTERNET>(1);
        running_.store(true, std::memory_order_release);
        recv_thread_ = std::thread(&WebSocketImpl::receive_loop_, this);
    }

private:
    bool test_receive_loop_started_ = false;

public:
    // Test-only hook: signals when receive_loop_() starts
    //
    // Debug builds exposed a race in the test harness.
    // Fixed it by adding a test-only synchronization hook to the transport so
    // tests wait on real transport state instead of timing assumptions.
    void set_receive_started_flag(std::atomic<bool>* flag) noexcept {
        receive_started_flag_ = flag;
    }

private:
    std::atomic<bool>* receive_started_flag_ = nullptr;
#endif // WK_UNIT_TEST

};

} // namespace wirekrak::core::transport::winhttp
