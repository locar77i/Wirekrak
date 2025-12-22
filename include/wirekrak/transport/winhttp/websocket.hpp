#pragma once

#include <windows.h>
#include <winhttp.h>

#include <string>
#include <thread>
#include <functional>
#include <atomic>
#include <vector>
#include <cassert>
#include <windows.h>

#include "wirekrak/transport/concepts.hpp"
#include "wirekrak/transport/winhttp/real_api.hpp"
#include "lcr/log/logger.hpp"


/*
================================================================================
WebSocket Transport (WinHTTP)
================================================================================

This header implements the WireKrak WebSocket transport using WinHTTP, following
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


namespace wirekrak {
namespace transport {
namespace winhttp {

template<ApiConcept Api = RealApi>
class WebSocketImpl {
    constexpr static size_t WINHTTP_WEB_SOCKET_BUFFER_MAX_SIZE = 16 * 1024;

public:
    using MessageCallback = std::function<void(const std::string&)>;
    using CloseCallback   = std::function<void()>;
    using ErrorCallback   = std::function<void(DWORD)>;

public:
    explicit WebSocketImpl() noexcept {
        hSession_ = WinHttpOpen(
            L"Wirekrak/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );
    }

    ~WebSocketImpl() {
        close();
        if (hSession_) {
            WinHttpCloseHandle(hSession_);
        }
    }

    [[nodiscard]]
    bool connect(const std::string& host,
                 const std::string& port,
                 const std::string& path) noexcept {
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
        recv_thread_ = std::thread(&WebSocketImpl::receive_loop, this);
        return true;
    }

    [[nodiscard]]
    bool send(const std::string& msg) noexcept {
        if (!hWebSocket_) {
            WK_ERROR("[WS] send() called on unconnected WebSocket");
            return false;
        }
        WK_DEBUG("[WS:API] Sending message ... (size " << msg.size() << ")");
        return api_.websocket_send(
                   hWebSocket_,
                   WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,
                   (void*)msg.data(),
                   (DWORD)msg.size()) == ERROR_SUCCESS;
    }

    void close() noexcept {
        // Stop the receive loop (idempotent)
        running_.store(false, std::memory_order_release);
        // Close the WebSocket (idempotent)
        if (hWebSocket_) {
            WK_DEBUG("[WS:API] Closing WebSocket ...");
            api_.websocket_close(hWebSocket_);
        }
        // Signal close callback (idempotent)
        signal_close_();
        // Join receive thread
        if (recv_thread_.joinable()) {
            recv_thread_.join();
        }
        if (hWebSocket_) { WinHttpCloseHandle(hWebSocket_); hWebSocket_ = nullptr; }
        if (hRequest_)   { WinHttpCloseHandle(hRequest_);   hRequest_ = nullptr; }
        if (hConnect_)   { WinHttpCloseHandle(hConnect_);   hConnect_ = nullptr; }
        if (hSession_)   { WinHttpCloseHandle(hSession_);   hSession_ = nullptr; }
        WK_TRACE("[WS] WebSocket closed.");
    }

    void set_message_callback(MessageCallback cb) noexcept { on_message_ = std::move(cb); }
    void set_close_callback(CloseCallback cb) noexcept     { on_close_   = std::move(cb); }
    void set_error_callback(ErrorCallback cb) noexcept     { on_error_   = std::move(cb); }

private:
    void receive_loop() noexcept {
#ifdef WK_UNIT_TEST
    // Debug builds exposed a race in the test harness.
    // Fixed it by adding a test-only synchronization hook to the transport so
    // tests wait on real transport state instead of timing assumptions.
    if (receive_started_flag_) {
        receive_started_flag_->store(true, std::memory_order_release);
    }
#endif // WK_UNIT_TEST
        std::vector<char> buffer(WINHTTP_WEB_SOCKET_BUFFER_MAX_SIZE);
        // Receive internal loop
        while (running_.load(std::memory_order_acquire)) {
            DWORD bytes = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
            WK_DEBUG("[WS:API] Receiving message ...");
            DWORD result = api_.websocket_receive(
                hWebSocket_,
                buffer.data(),
                (DWORD)buffer.size(),
                &bytes,
                &type
            );
            // Handle errors
            if (result != ERROR_SUCCESS) { // abnormal termination
                if (on_error_) {
                    on_error_(result);
                }
                running_.store(false, std::memory_order_release);
                signal_close_();
                break;
            }
            // Handle close frame
            if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) { // normal termination
                running_.store(false, std::memory_order_release);
                signal_close_();
                break;
            }
            // Handle message
            if (type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
                if (on_message_) {
                    on_message_(std::string(buffer.data(), bytes));
                }
            }
        }
    }

    void signal_close_() noexcept {
        if (closed_.exchange(true)) {
            return;
        }
        if (on_close_) {
            on_close_();
        }
    }

private:
    Api api_;

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
        recv_thread_ = std::thread(&WebSocketImpl::receive_loop, this);
    }

private:
    bool test_receive_loop_started_ = false;

public:
    // Test-only hook: signals when receive_loop() starts
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
static_assert(wirekrak::transport::WebSocketConcept<WebSocket>);

} // namespace winhttp
} // namespace transport
} // namespace wirekrak
