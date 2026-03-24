#pragma once

// ============================================================================
// WinHTTP WebSocket Backend
// ============================================================================
//
// Overview
// --------
// This backend provides a WebSocket transport implementation using the
// Windows WinHTTP API. It is intended primarily for compatibility and ease of
// deployment on Windows systems where Boost.Asio or lower-level socket access
// may not be desirable or available.
//
// Characteristics
// ---------------
// - Blocking, synchronous API
// - Minimal dependencies (Windows-native)
// - Simple integration with system networking stack
//
// Limitations (Important)
// ----------------------
// This backend does NOT provide deterministic or bounded-latency behavior.
// In particular:
//
// 1. Receive Interruptibility
//    - `WinHttpWebSocketReceive` is a blocking call.
//    - Calls to `WinHttpWebSocketClose` or `WinHttpCloseHandle` do NOT guarantee
//      immediate interruption of a pending receive operation.
//    - Shutdown latency may vary from microseconds to several seconds depending
//      on OS, network state, and internal WinHTTP behavior.
//
// 2. Non-Deterministic Shutdown
//    - The backend does not guarantee bounded-time shutdown.
//    - Threads blocked in `read_some()` may delay transport termination.
//
// 3. Not Suitable for ULL (Ultra-Low Latency)
//    - Due to the above properties, this backend is NOT suitable for:
//        * latency-sensitive trading systems
//        * deterministic protocol execution
//        * high-frequency data ingestion pipelines
//
// Design Positioning
// ------------------
// This backend is provided as a compatibility layer and reference implementation.
// It prioritizes portability and simplicity over performance and determinism.
//
// For systems requiring strict correctness guarantees, bounded latency, and
// precise control over I/O behavior, use the Asio-based backend or a custom
// socket implementation.
//
// Backend Contract Notes
// ----------------------
// - `close()` is idempotent and thread-safe (guarded internally).
// - `read_some()` follows the BackendConcept contract, but may block
//   uninterruptibly for an unbounded duration.
// - Error reporting is best-effort and dependent on WinHTTP error codes.
//
// Summary
// -------
// ✔ Easy to use
// ✔ Windows-native
// ❌ Not deterministic
// ❌ Not interruptible in bounded time
// ❌ Not suitable for ULL systems
//
// ============================================================================

#include <windows.h>
#include <winhttp.h>
#include <winerror.h>

#include <string>
#include <string_view>
#include <cstdint>
#include <atomic>

#include "wirekrak/core/transport/websocket/backend_concept.hpp"


namespace wirekrak::core::transport {
namespace winhttp {

// -------------------------------------------------------------
// Helper: UTF-8 → UTF-16
// -------------------------------------------------------------
inline std::wstring to_wide(std::string_view utf8) {
    if (utf8.empty()) return {};

    int size = MultiByteToWideChar(CP_UTF8, 0,
        utf8.data(), static_cast<int>(utf8.size()),
        nullptr, 0);

    if (size <= 0) return {};

    std::wstring out(size, L'\0');

    MultiByteToWideChar(CP_UTF8, 0,
        utf8.data(), static_cast<int>(utf8.size()),
        out.data(), size);

    return out;
}

// -------------------------------------------------------------
// WinHTTP Backend (BackendConcept)
// -------------------------------------------------------------
class Backend {
public:
    bool connect(std::string_view host,
                 std::uint16_t port,
                 std::string_view path,
                 bool secure) noexcept
    {
        host_w_ = to_wide(host);
        path_w_ = to_wide(path);

        hSession_ = WinHttpOpen(
            L"Wirekrak/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (!hSession_) return false;

        hConnect_ = WinHttpConnect(hSession_, host_w_.c_str(), port, 0);
        if (!hConnect_) return false;

        DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;

        hRequest_ = WinHttpOpenRequest(
            hConnect_,
            L"GET",
            path_w_.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags);

        if (!hRequest_) return false;

        if (!WinHttpSetOption(
                hRequest_,
                WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,
                nullptr,
                0)) return false;

        if (!WinHttpSendRequest(hRequest_, nullptr, 0, nullptr, 0, 0, 0))
            return false;

        if (!WinHttpReceiveResponse(hRequest_, nullptr))
            return false;

        hWebSocket_ = WinHttpWebSocketCompleteUpgrade(hRequest_, 0);
        return hWebSocket_ != nullptr;
    }

    void close() noexcept {
        // Fast path: ensure only one thread executes shutdown
        if (closed_.exchange(true, std::memory_order_acq_rel)) [[unlikely]] {
            return;
        }

        // Interrupt receive ASAP
        if (hWebSocket_) {
            WinHttpWebSocketClose(
                hWebSocket_,
                WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
                nullptr,
                0);
        }

        // Then close handles
        if (hWebSocket_) { WinHttpCloseHandle(hWebSocket_); hWebSocket_ = nullptr; }
        if (hRequest_)   { WinHttpCloseHandle(hRequest_);   hRequest_ = nullptr; }
        if (hConnect_)   { WinHttpCloseHandle(hConnect_);   hConnect_ = nullptr; }
        if (hSession_)   { WinHttpCloseHandle(hSession_);   hSession_ = nullptr; }
    }

    bool is_open() const noexcept {
        return hWebSocket_ != nullptr;
    }

    bool send(std::string_view msg) noexcept {
        if (!hWebSocket_) return false;

        DWORD result = WinHttpWebSocketSend(
            hWebSocket_,
            WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
            const_cast<char*>(msg.data()),
            static_cast<DWORD>(msg.size()));

        return result == ERROR_SUCCESS;
    }

    websocket::ReceiveStatus read_some(
        void* buffer,
        std::size_t size,
        std::size_t& bytes) noexcept
    {
        DWORD read = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE type{};

        DWORD result = WinHttpWebSocketReceive(
            hWebSocket_,
            buffer,
            static_cast<DWORD>(size),
            &read,
            &type);

        bytes = read;

        if (result != ERROR_SUCCESS) {
            return map_error_(result);
        }

        last_type_ = type;

        if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
            return websocket::ReceiveStatus::Closed;
        }

        return websocket::ReceiveStatus::Ok;
    }

    bool message_done() const noexcept {
        return last_type_ == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE ||
               last_type_ == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
    }

private:
    websocket::ReceiveStatus map_error_(DWORD err) noexcept {
        switch (err) {
            case ERROR_WINHTTP_OPERATION_CANCELLED:
                return websocket::ReceiveStatus::Closed;

            case ERROR_WINHTTP_CONNECTION_ERROR:
                return websocket::ReceiveStatus::Closed;

            case ERROR_WINHTTP_TIMEOUT:
                return websocket::ReceiveStatus::Timeout;

            case ERROR_WINHTTP_INVALID_SERVER_RESPONSE:
                return websocket::ReceiveStatus::ProtocolError;

            default:
                return websocket::ReceiveStatus::TransportError;
        }
    }

private:
    std::wstring host_w_;
    std::wstring path_w_;

    HINTERNET hSession_   = nullptr;
    HINTERNET hConnect_   = nullptr;
    HINTERNET hRequest_   = nullptr;
    HINTERNET hWebSocket_ = nullptr;

    WINHTTP_WEB_SOCKET_BUFFER_TYPE last_type_{};

    std::atomic<bool> closed_{false};
};

} // namespace winhttp

// Assert BackendConcept compliance at compile-time
static_assert(websocket::BackendConcept<winhttp::Backend>, "winhttp::Backend does not satisfy websocket::BackendConcept");

} // namespace wirekrak::core::transport
