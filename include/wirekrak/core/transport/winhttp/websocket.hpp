#pragma once

#include <string>
#include <string_view>
#include <thread>
#include <functional>
#include <atomic>
#include <vector>
#include <cassert>

#include "wirekrak/core/transport/concepts.hpp"
#include "wirekrak/core/transport/winhttp/real_api.hpp"
#include "wirekrak/core/transport/telemetry/websocket.hpp"
#include "wirekrak/core/telemetry.hpp"
#include "lcr/system/monotonic_clock.hpp"
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


namespace wirekrak::core {
namespace transport {
namespace winhttp {

template<ApiConcept Api = RealApi>
class WebSocketImpl {

    // The receive buffer will be sized for the common case (not the worst case). Why RX_BUFFER_SIZE = 8 KB:
    // - fits comfortably in L1/L2 cache
    // - covers >99% of messages in one call
    // - snapshots still handled correctly
    // - fragmentation remains rare
    // - minimal memory waste
    //
    // Telemetry shows 8–16 KB is optimal: 
    // Big enough to hold the 99th percentile message comfortably, small enough to stay cache-friendly.
    // 8 KB buffers give us the best balance of cache locality and correctness, with no measurable downside for Kraken traffic.
    constexpr static size_t RX_BUFFER_SIZE = 8 * 1024;

public:
    using MessageCallback = std::function<void(std::string_view)>;
    using CloseCallback   = std::function<void()>;
    using ErrorCallback   = std::function<void(DWORD)>;

public:
    explicit WebSocketImpl(telemetry::WebSocket& telemetry) noexcept
        : telemetry_(telemetry) {
        message_buffer_.reserve(RX_BUFFER_SIZE);
    }

    ~WebSocketImpl() {
        close();
        if (hSession_) {
            WinHttpCloseHandle(hSession_);
            hSession_ = nullptr;
        }
    }

    [[nodiscard]]
    inline bool connect(const std::string& host,
                 const std::string& port,
                 const std::string& path) noexcept {
        hSession_ = WinHttpOpen(
            L"Wirekrak/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
        if (!hSession_) {
            WK_ERROR("[WS] WinHttpOpen failed");
            return false;
        }

        hConnect_ = WinHttpConnect(
            hSession_,
            to_wide(host).c_str(),
            std::stoi(port),
            0
        );
        if (!hConnect_) {
            WK_ERROR("[WS] WinHttpConnect failed");
            return false;
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
            return false;
        }

        if (!WinHttpSetOption(hRequest_, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) {
            WK_ERROR("[WS] WinHttpSetOption failed");
            return false;
        }

        if (!WinHttpSendRequest(hRequest_, nullptr, 0, nullptr, 0, 0, 0)) {
            WK_ERROR("[WS] WinHttpSendRequest failed");
            return false;
        }

        if (!WinHttpReceiveResponse(hRequest_, nullptr)) {
            WK_ERROR("[WS] WinHttpReceiveResponse failed");
            return false;
        }

        hWebSocket_ = WinHttpWebSocketCompleteUpgrade(hRequest_, 0);
        if (!hWebSocket_) {
            WK_ERROR("[WS] WinHttpWebSocketCompleteUpgrade failed");
            return false;
        }

        running_.store(true, std::memory_order_relaxed);
        recv_thread_ = std::thread(&WebSocketImpl::receive_loop_, this);
        return true;
    }

    [[nodiscard]]
    inline bool send(const std::string& msg) noexcept {
        if (!hWebSocket_) {
            WK_ERROR("[WS] send() called on unconnected WebSocket");
            return false;
        }
        WK_TRACE("[WS:API] Sending message ... (size " << msg.size() << ")");
        const bool ok = api_.websocket_send(
                   hWebSocket_,
                   WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,
                   (void*)msg.data(),
                   (DWORD)msg.size()) == ERROR_SUCCESS;
        WK_TL1( if (ok) [[likely]] { telemetry_.on_send(msg.size()); } );
        return ok;
    }

    inline void close() noexcept {
        // Close the WebSocket (idempotent)
        if (hWebSocket_) {
            WK_TRACE("[WS:API] Closing WebSocket ...");
            api_.websocket_close(hWebSocket_);
        }
        // Stop the receive loop (idempotent)
        running_.store(false, std::memory_order_release);
        // Signal close callback (idempotent)
        signal_close_();
        // Join receive thread
        if (recv_thread_.joinable()) {
            recv_thread_.join();
        }
        if (hWebSocket_) { WinHttpCloseHandle(hWebSocket_); hWebSocket_ = nullptr; }
        if (hRequest_)   { WinHttpCloseHandle(hRequest_);   hRequest_ = nullptr; }
        if (hConnect_)   { WinHttpCloseHandle(hConnect_);   hConnect_ = nullptr; }
        //if (hSession_)   { WinHttpCloseHandle(hSession_);   hSession_ = nullptr; }
        WK_TRACE("[WS] WebSocket closed.");
    }

    inline void set_message_callback(MessageCallback cb) noexcept     { on_message_ = std::move(cb); }
    inline void set_close_callback(CloseCallback cb) noexcept         { on_close_   = std::move(cb); }
    inline void set_error_callback(ErrorCallback cb) noexcept         { on_error_   = std::move(cb); }

private:
    inline void receive_loop_() noexcept {
#ifdef WK_UNIT_TEST
        // Debug builds exposed a race in the test harness.
        // Fixed it by adding a test-only synchronization hook to the transport so
        // tests wait on real transport state instead of timing assumptions.
        if (receive_started_flag_) {
            receive_started_flag_->store(true, std::memory_order_release);
        }
#endif // WK_UNIT_TEST
        std::vector<char> buffer(RX_BUFFER_SIZE);
        std::string message;
        WK_TL1( uint32_t fragments = 0 );
        // Receive internal loop
        while (running_.load(std::memory_order_acquire)) {
            DWORD bytes = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
            WK_TRACE("[WS:API] Receiving message ...");
            DWORD result = api_.websocket_receive(
                hWebSocket_,
                buffer.data(),
                (DWORD)buffer.size(),
                &bytes,
                &type
            );
            // Handle errors
            if (result != ERROR_SUCCESS) [[unlikely]] { // abnormal termination
            WK_TL1( telemetry_.on_receive_failure() );
                handle_receive_error_(result);
                if (on_error_) {
                    on_error_(result);
                }
                running_.store(false, std::memory_order_release);
                signal_close_();
                break;
            }
            else { // successful receive
                WK_TL1( telemetry_.on_receive(bytes) );
            }
            // Handle close frame
            if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) { // normal termination
                WK_INFO("[WS] Received WebSocket close frame.");
                running_.store(false, std::memory_order_release);
                signal_close_();
                break;
            }
            // Handle final message frame
            if (type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)  [[likely]] {
                if (message_buffer_.empty()) { // single-frame message
                    if (on_message_) {
                        on_message_(std::string_view(buffer.data(), bytes));
                    }
                    WK_TL1( telemetry_.on_receive_message(bytes, 1) );
                }
                else { // completing fragmented message
                    WK_TL3( const auto t0 = lcr::system::monotonic_clock::instance().now_ns() );
                    message_buffer_.append(buffer.data(), bytes);
                    WK_TL3( telemetry_.on_message_assembly_copy(lcr::system::monotonic_clock::instance().now_ns() - t0) );
                    WK_TL1( telemetry_.on_receive_message(message_buffer_.size(), fragments + 1); fragments = 0 );
                    if (on_message_) {
                        on_message_(std::string_view(message_buffer_.data(), message_buffer_.size()));
                    }
                    message_buffer_.clear();
                }
                
            }
            else // Handle message fragments
            if (type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE) {
                WK_DEBUG("[WS] Received message fragment (size " << bytes << ")");
                WK_TL3( const auto t0 = lcr::system::monotonic_clock::instance().now_ns() );
                message_buffer_.append(buffer.data(), bytes);
                WK_TL3( telemetry_.on_message_assembly_copy(lcr::system::monotonic_clock::instance().now_ns() - t0) );
                // Track fragments for telemetry
                WK_TL1( telemetry_.on_message_assembly(message_buffer_.size()); ++fragments );
            }
        }
    }

    inline void handle_receive_error_(DWORD error) noexcept {
        // WinHTTP uses WinINet-style numeric error codes
        constexpr DWORD ERR_OPERATION_CANCELLED  = 12017; // local close
        constexpr DWORD ERR_CONNECTION_ABORTED   = 12030; // peer closed
        constexpr DWORD ERR_CANNOT_CONNECT       = 12029; // connect failed
        constexpr DWORD ERR_TIMEOUT              = 12002; // timeout

        switch (error) {
        case ERR_OPERATION_CANCELLED:
            // Local shutdown, expected during close()
            WK_TRACE("[WS] Receive cancelled (local shutdown)");
            break;

        case ERR_CONNECTION_ABORTED:
            // Remote closed connection (no CLOSE frame)
            WK_INFO("[WS] Connection closed by peer");
            break;

        case ERR_TIMEOUT:
            // Network stalled or idle timeout
            WK_WARN("[WS] Receive timeout");
            break;

        case ERR_CANNOT_CONNECT:
            // Usually handshake or DNS issues
            WK_ERROR("[WS] Cannot connect to remote host");
            break;

        default:
            // Anything else is unexpected
            WK_ERROR("[WS] Receive failed with error code " << error);
            break;
        }
    }

    inline void signal_close_() noexcept {
        message_buffer_.clear();
        if (closed_.exchange(true)) {
            return;
        }
        WK_TL1( telemetry_.on_close_event() );
        if (on_close_) {
            on_close_();
        }
    }

private:
    telemetry::WebSocket& telemetry_;
    Api api_;

    std::string message_buffer_{};

    HINTERNET hSession_   = nullptr;
    HINTERNET hConnect_   = nullptr;
    HINTERNET hRequest_   = nullptr;
    HINTERNET hWebSocket_ = nullptr;

    std::thread recv_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> closed_{false};

    MessageCallback on_message_;
    CloseCallback   on_close_;
    ErrorCallback   on_error_;

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
// Defensive check that WebSocket conforms to the transport::WebSocketConcept concept
using WebSocket = WebSocketImpl<RealApi>;
static_assert(wirekrak::core::transport::WebSocketConcept<WebSocket>);

} // namespace winhttp
} // namespace transport
} // namespace wirekrak::core
