#pragma once

#include <windows.h>
#include <winhttp.h>
#include <concepts>


namespace wirekrak::core {
namespace transport {
namespace winhttp {

// -------------------------------------------------------------------------
// ApiConcept defines the minimal WinHTTP WebSocket API surface required
// by the transport layer. This enables dependency injection, testing,
// and zero-overhead abstraction via C++20 concepts.
// -------------------------------------------------------------------------

template<class T>
concept ApiConcept = requires(
    T api,
    HINTERNET ws,
    void* buffer,
    DWORD size,
    DWORD* bytes,
    WINHTTP_WEB_SOCKET_BUFFER_TYPE* type
) {
    { api.websocket_receive(ws, buffer, size, bytes, type) } -> std::same_as<DWORD>;
    { api.websocket_send(ws, WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, buffer, size) } -> std::same_as<DWORD>;
    { api.websocket_close(ws) } -> std::same_as<void>;
};

} // namespace winhttp
} // namespace transport
} // namespace wirekrak::core
