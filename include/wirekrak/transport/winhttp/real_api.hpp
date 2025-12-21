#pragma once

#include "wirekrak/transport/winhttp/concepts.hpp"


namespace wirekrak {
namespace transport {
namespace winhttp {

struct RealApi {
    DWORD websocket_receive(HINTERNET ws, void* buffer, DWORD size, DWORD* bytes, WINHTTP_WEB_SOCKET_BUFFER_TYPE* type) noexcept {
        return WinHttpWebSocketReceive(ws, buffer, size, bytes, type);
    }

    DWORD websocket_send(HINTERNET ws, WINHTTP_WEB_SOCKET_BUFFER_TYPE type, void* buffer, DWORD size) noexcept {
        return WinHttpWebSocketSend(ws, type, buffer, size);
    }

    void websocket_close(HINTERNET ws) noexcept {
        WinHttpWebSocketClose(ws, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
    }
};
static_assert(ApiConcept<RealApi>, "RealApi must model ApiConcept");

} // namespace winhttp
} // namespace transport
} // namespace wirekrak
