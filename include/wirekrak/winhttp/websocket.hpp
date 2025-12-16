#pragma once

#include <windows.h>
#include <winhttp.h>

#include <string>
#include <thread>
#include <functional>
#include <atomic>
#include <vector>
#include <windows.h>

#include "wirekrak/core/transport/websocket.hpp"


// helper to convert UTF-8 string to wide string
inline std::wstring to_wide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    std::wstring out(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &out[0], size);
    return out;
}


namespace wirekrak {
namespace winhttp {

// WebSocket implementation using WinHTTP
// WireKrak cleanly separates transport and policy. The WebSocket is responsible only for:
// - connect
// - send
// - receive
// - close
// - signal failure
//
// The WinHTTP WebSocket is a single-connection primitive that only signals failure.
// Reconnection and recovery are handled at the client level through a deterministic state machine.
class WebSocket {
    // TODO: Reduce buffer size until BUG (incomplete messages) appears
    constexpr static size_t WINHTTP_WEB_SOCKET_BUFFER_MAX_SIZE = 16 * 1024; // 16 KB

public:
    using MessageCallback = std::function<void(const std::string&)>;
    using CloseCallback   = std::function<void()>;
    using ErrorCallback   = std::function<void(DWORD)>;

public:

    WebSocket() {
        // Initialize WinHTTP session: User-Agent: Wirekrak/1.0
        hSession_ = WinHttpOpen(L"Wirekrak/1.0",
                                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS,
                                0);
    }

    ~WebSocket() {
        close();
        if (hSession_) {
            WinHttpCloseHandle(hSession_);
        }
    }

    // Connection Lifecycle (Single Attempt). Rules:
    // - connect() = one attempt
    // - No retries
    // - Either succeeds or fail
    [[nodiscard]]
    inline bool connect(const std::string& host, const std::string& port, const std::string& path) noexcept {
        if (!hSession_) {
            return false;
        }
        // Convert to wide strings
        std::wstring host_w = to_wide(host);
        std::wstring port_w = to_wide(port);
        std::wstring path_w = to_wide(path);
        // Create connection
        hConnect_ = WinHttpConnect(hSession_, host_w.c_str(), std::stoi(port_w), 0);
        if (!hConnect_) return false;
        // Create request
        hRequest_ = WinHttpOpenRequest(
            hConnect_,
            L"GET",
            path_w.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE
        );
        if (!hRequest_) {
            return false;
        }
        // Upgrade to WebSocket
        BOOL ok = WinHttpSetOption(hRequest_, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0);
        if (!ok) return false;

        ok = WinHttpSendRequest(hRequest_, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0);
        if (!ok) return false;

        ok = WinHttpReceiveResponse(hRequest_, nullptr);
        if (!ok) return false;

        hWebSocket_ = WinHttpWebSocketCompleteUpgrade(hRequest_, (DWORD_PTR)0);
        if (!hWebSocket_) return false;

        closed_ = false;
        running_ = true;
        recv_thread_ = std::thread(&WebSocket::receive_loop, this);

        return true;
    }

    // Send (Fire-and-Forget). Rules:
    // - Fail fast if socket closed
    // - No buffering
    // - No retry
    // - No reconnection
    [[nodiscard]]
    inline bool send(const std::string& msg) noexcept {
        if (!hWebSocket_) return false;

        WINHTTP_WEB_SOCKET_BUFFER_TYPE type = WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;

        DWORD result = WinHttpWebSocketSend(
            hWebSocket_,
            type,
            (void*)msg.data(),
            (DWORD)msg.size()
        );

        return result == ERROR_SUCCESS;
    }

    inline void close() noexcept {
        if (!running_) {
            return;
        }
        running_ = false;
        // Close WebSocket gracefully
        if (hWebSocket_) {
            WinHttpWebSocketClose(hWebSocket_, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);
        }
        // Ensure receive loop has exited
        signal_close_();
        // Wait for receive thread to finish
        if (recv_thread_.joinable()) {
            recv_thread_.join();
        }
        // Close WinHTTP handles
        if (hWebSocket_) {
            WinHttpCloseHandle(hWebSocket_);
            hWebSocket_ = NULL;
        }
        if (hRequest_)  {
            WinHttpCloseHandle(hRequest_);
            hRequest_ = NULL;
        }
        if (hConnect_)  {
            WinHttpCloseHandle(hConnect_);
            hConnect_ = NULL;
        }
        if (hSession_)  {
            WinHttpCloseHandle(hSession_);
            hSession_ = NULL;
        }
        closed_ = true;
    }

    // Failure Detection & Signaling
    inline void set_message_callback(MessageCallback cb) noexcept {
        on_message_ = std::move(cb);
    }

    inline void set_close_callback(CloseCallback cb) noexcept {
        on_close_ = std::move(cb);
    }

    inline void set_error_callback(ErrorCallback cb) noexcept {
        on_error_ = std::move(cb);
    }

private:
    // Receive Loop (Blocking). Acceptable at transport level because:
    // - WinHTTP is blocking
    // - This thread is not “hidden reconnection”
    // - It maps 1:1 to OS I/O
    inline void receive_loop() noexcept {
        std::vector<char> buffer(WINHTTP_WEB_SOCKET_BUFFER_MAX_SIZE);

        while (running_) {
            // Receive message
            DWORD bytes = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
            DWORD result = WinHttpWebSocketReceive(
                hWebSocket_,
                buffer.data(),
                (DWORD)buffer.size(),
                &bytes,
                &type
            );
            // Handle error
            if (result != ERROR_SUCCESS) {
                signal_close_();
                if (on_error_) {
                    on_error_(result);
                }
                break;
            }
            // Handle close message
            if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
                signal_close_();
                break;  
            }
            // Handle normal message
            if (type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
                if (on_message_) {
                    on_message_(std::string(buffer.data(), bytes));
                }
            }
        }

        running_ = false;
    }

    void signal_close_() {
        if (closed_.exchange(true)) {
            return;
        }
        running_ = false;
        if (on_close_) {
            on_close_();
        }
    }

private:
    HINTERNET hSession_ = NULL;
    HINTERNET hConnect_ = NULL;
    HINTERNET hRequest_ = NULL;
    HINTERNET hWebSocket_ = NULL;

    std::thread recv_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> closed_{false};

    MessageCallback on_message_;
    CloseCallback on_close_;
    ErrorCallback on_error_;
};
// Defensive check that WebSocket conforms to the transport::WebSocket concept
static_assert(wirekrak::transport::WebSocket<WebSocket>);


} // namespace winhttp
} // namespace wirekrak
