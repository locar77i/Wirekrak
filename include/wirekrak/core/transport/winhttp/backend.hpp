#pragma once

#include <windows.h>
#include <winhttp.h>
#include <winerror.h>

#include <string>
#include <string_view>
#include <cstdint>

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
        if (hWebSocket_) {
            WinHttpWebSocketClose(
                hWebSocket_,
                WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
                nullptr,
                0);
        }

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
};

} // namespace winhttp

// Assert BackendConcept compliance at compile-time
static_assert(websocket::BackendConcept<winhttp::Backend>, "winhttp::Backend does not satisfy websocket::BackendConcept");

} // namespace wirekrak::core::transport
