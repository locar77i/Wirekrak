#pragma once

// ============================================================================
// WinHTTP WebSocket Backend
// ============================================================================
//
// Overview
// --------
// This backend provides a WebSocket transport implementation using the
// Windows WinHTTP API. It is designed as a compatibility-oriented adapter
// that conforms to the Wirekrak BackendConcept while normalizing WinHTTP
// behavior into deterministic transport semantics.
//
// This is NOT a thin wrapper over WinHTTP. The backend enforces strict
// invariants required by the transport layer and hides platform-specific
// inconsistencies behind a stable contract.
//
// Characteristics
// ---------------
// - Blocking, synchronous API (WinHTTP-based)
// - Minimal dependencies (Windows-native)
// - Contract-enforcing adapter (not a direct passthrough)
// - Zero-copy compatible (caller-owned buffers)
//
// Semantic Normalization
// ----------------------
// This backend does not expose raw WinHTTP semantics directly. Instead,
// it normalizes behavior to satisfy the BackendConcept contract:
//
// - All CLOSE conditions (including selected WinHTTP errors such as
//   connection loss or cancellation) are mapped to:
//       { status = Ok, frame = Close }
//
// - CLOSE frames are guaranteed to have:
//       bytes == 0
//   even if WinHTTP provides payload data.
//
// - No bytes are ever returned when:
//       status != Ok
//
// - Fragmentation and message boundaries are explicitly mapped to:
//       FrameType::{Fragment, Message, Close}
//
// This ensures consistent behavior across different backend implementations
// and simplifies transport-layer logic.
//
// Determinism Model
// -----------------
// - Underlying I/O (WinHTTP) is NOT deterministic:
//     * WinHttpWebSocketReceive is blocking
//     * Cancellation is not guaranteed to be immediate
//
// - However, this backend provides deterministic *contract behavior*:
//     * All outputs strictly satisfy BackendConcept invariants
//     * No undefined or ambiguous states are exposed to the transport layer
//
// Shutdown Semantics
// ------------------
// - close() is idempotent and thread-safe
// - A local atomic flag (`closed_`) is used to enforce fast logical shutdown
//
// After close():
//   - read_some() returns immediately with:
//         { status = Ok, frame = Close, bytes = 0 }
//   - send() fails fast
//
// NOTE:
// Threads blocked inside WinHttpWebSocketReceive may still experience
// unbounded latency before returning. This is a WinHTTP limitation and
// cannot be fully controlled by the backend.
//
// Error Model
// -----------
// - WinHTTP errors are mapped into BackendConcept semantics
// - Certain transport-level failures are intentionally normalized:
//
//     ERROR_WINHTTP_CONNECTION_ERROR
//     ERROR_WINHTTP_OPERATION_CANCELLED
//         → treated as graceful CLOSE
//
// - Other errors are mapped to:
//     * Timeout
//     * ProtocolError
//     * TransportError
//
// This design favors semantic consistency over strict preservation of
// underlying OS error meanings. Higher layers (e.g., retry policies)
// are responsible for recovery decisions.
//
// Limitations
// -----------
// - Not suitable for Ultra-Low Latency (ULL) systems
// - Blocking receive cannot be interrupted in bounded time
// - Shutdown latency depends on WinHTTP internal behavior
//
// Design Positioning
// ------------------
// This backend is intended as:
//   ✔ A portable, Windows-native implementation
//   ✔ A reference backend for BackendConcept compliance
//
// It is NOT intended for:
//   ✘ Latency-critical systems
//   ✘ Deterministic I/O scheduling
//   ✘ High-frequency trading pipelines
//
// For strict latency and control requirements, prefer:
//   - Asio-based backend
//   - Custom socket-level implementations
//
// Summary
// -------
// ✔ BackendConcept-compliant
// ✔ Deterministic transport semantics
// ✔ Normalized cross-platform behavior
// ✔ Windows-native and dependency-light
//
// ❌ Non-deterministic underlying I/O
// ❌ Unbounded blocking in receive
// ❌ Not suitable for ULL workloads
//
// ============================================================================

#include <winsock2.h> // Must be included before ASIO on Windows
#include <windows.h>
#include <winhttp.h>
#include <winerror.h>

#include <string>
#include <string_view>
#include <cstdint>
#include <atomic>
#include <limits>
#include <algorithm>

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
    bool connect(std::string_view host, std::uint16_t port, std::string_view path, bool secure) noexcept {
        
        closed_.store(false, std::memory_order_release);

        host_w_ = to_wide(host);
        path_w_ = to_wide(path);

        hSession_ = WinHttpOpen(
            L"Wirekrak/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (!hSession_) {
            cleanup_(); // safe, idempotent
            return false;
        }

        hConnect_ = WinHttpConnect(hSession_, host_w_.c_str(), port, 0);
        if (!hConnect_) {
            cleanup_();
            return false;
        }

        DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;

        hRequest_ = WinHttpOpenRequest(
            hConnect_,
            L"GET",
            path_w_.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags);

        if (!hRequest_) {
            cleanup_();
            return false;
        }

        if (!WinHttpSetOption(hRequest_, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) {
            cleanup_();
            return false;
        }

        if (!WinHttpSendRequest(hRequest_, nullptr, 0, nullptr, 0, 0, 0)) {
            cleanup_();
            return false;
        }

        if (!WinHttpReceiveResponse(hRequest_, nullptr)) {
            cleanup_();
            return false;
        }

        hWebSocket_ = WinHttpWebSocketCompleteUpgrade(hRequest_, 0);
        if (!hWebSocket_) {
            cleanup_();
            return false;
        }
        return true;
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
        cleanup_();
    }

    bool is_open() const noexcept {
        return !closed_.load(std::memory_order_acquire) && hWebSocket_ != nullptr;
    }

    bool send(std::string_view msg) noexcept {
        // =========================================================
        // Preconditions
        // =========================================================
        if (closed_.load(std::memory_order_acquire)) [[unlikely]] {
            return false;
        };

        if (!hWebSocket_) {
            return false;
        }

        DWORD result = WinHttpWebSocketSend(
            hWebSocket_,
            WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
            const_cast<char*>(msg.data()),
            static_cast<DWORD>(msg.size()));

        return result == ERROR_SUCCESS;
    }

    websocket::ReadResult read_some(void* buffer, std::size_t size) noexcept {

        // =========================================================
        // Preconditions
        // =========================================================
        if (closed_.load(std::memory_order_acquire)) [[unlikely]] {
            return websocket::ReadResult{
                .status = websocket::ReceiveStatus::Ok,
                .bytes = 0,
                .frame = websocket::FrameType::Close
            };
        };

        if (!hWebSocket_) [[unlikely]] { // If the socket is gone -> this is effectively a CLOSE:
            return websocket::ReadResult{
                .status = websocket::ReceiveStatus::Ok,
                .bytes = 0,
                .frame = websocket::FrameType::Close
            };
        }

        // =========================================================
        // Perform receive
        // =========================================================
        DWORD read = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE type{};

        DWORD win_size = static_cast<DWORD>(std::min<std::size_t>(size, std::numeric_limits<DWORD>::max()));
        DWORD result = WinHttpWebSocketReceive(
            hWebSocket_,
            buffer,
            win_size,
            &read,
            &type
        );

        // =========================================================
        // Error path
        // =========================================================
        if (result != ERROR_SUCCESS) [[unlikely]] {
            return map_result_on_error_(result);
        }

        // =========================================================
        // Map frame type
        // =========================================================
        using FT = websocket::FrameType;

        FT frame;

        switch (type) {
            case WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE:
            case WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE:
                frame = FT::Fragment;
                break;

            case WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE:
            case WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE:
                frame = FT::Message;
                break;

            case WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE:
                frame = FT::Close;
                break;

            default:
                // Should never happen, but be defensive
                return websocket::ReadResult{
                    .status = websocket::ReceiveStatus::ProtocolError,
                    .bytes = 0,
                    .frame = FT::Fragment
                };
        }

        // =========================================================
        // Enforce contract invariants
        // =========================================================

        // Close frame MUST have 0 bytes
        // WinHTTP may return payload for CLOSE frames but we discard it to enforce transport invariant (bytes == 0).
        if (frame == FT::Close) {
            read = 0;
        }

        return websocket::ReadResult{
            .status = websocket::ReceiveStatus::Ok,
            .bytes = static_cast<std::size_t>(read),
            .frame = frame
        };
    }

private:
    websocket::ReadResult map_result_on_error_(DWORD err) noexcept {
        switch (err) {
            case ERROR_WINHTTP_TIMEOUT:
                return websocket::ReadResult{
                .status = websocket::ReceiveStatus::Timeout,
                .bytes = 0,
                .frame = websocket::FrameType::Fragment // dummy, ignored
            };

            case ERROR_WINHTTP_INVALID_SERVER_RESPONSE:
                return websocket::ReadResult{
                .status = websocket::ReceiveStatus::ProtocolError,
                .bytes = 0,
                .frame = websocket::FrameType::Fragment // dummy, ignored
            };

            case ERROR_WINHTTP_CONNECTION_ERROR: // -> Close
                // This is correct in practice, but it's not strictly guaranteed by WinHTTP and could also mean abrupt disconnect (current approach = correct trade-off)
                // NOTE:
                // ERROR_WINHTTP_CONNECTION_ERROR may represent abrupt disconnect.
                // We normalize it to CLOSE to keep transport semantics simple.
                // Higher layers (retry policy) are responsible for recovery.
            case ERROR_WINHTTP_OPERATION_CANCELLED: // -> Close
                return websocket::ReadResult{ // Already decided -> close is not an error (enforce graceful close semantics)
                    .status = websocket::ReceiveStatus::Ok,
                    .bytes = 0,
                    .frame = websocket::FrameType::Close // Treat as graceful close equivalent but signaling Close frame instead
                };

            default:
                return websocket::ReadResult{
                    .status = websocket::ReceiveStatus::TransportError,
                    .bytes = 0,
                    .frame = websocket::FrameType::Fragment // dummy, ignored
                };
        }
    }

    void cleanup_() noexcept {
        if (hWebSocket_) { WinHttpCloseHandle(hWebSocket_); hWebSocket_ = nullptr; }
        if (hRequest_)   { WinHttpCloseHandle(hRequest_);   hRequest_ = nullptr; }
        if (hConnect_)   { WinHttpCloseHandle(hConnect_);   hConnect_ = nullptr; }
        if (hSession_)   { WinHttpCloseHandle(hSession_);   hSession_ = nullptr; }
    }

private:
    std::wstring host_w_;
    std::wstring path_w_;

    HINTERNET hSession_   = nullptr;
    HINTERNET hConnect_   = nullptr;
    HINTERNET hRequest_   = nullptr;
    HINTERNET hWebSocket_ = nullptr;

    std::atomic<bool> closed_{false};
};

} // namespace winhttp

// Assert BackendConcept compliance at compile-time
static_assert(websocket::BackendConcept<winhttp::Backend>, "winhttp::Backend does not satisfy websocket::BackendConcept");

} // namespace wirekrak::core::transport
