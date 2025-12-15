#include <iostream>

#include "wirekrak/winhttp/websocket.hpp"


namespace wirekrak {
namespace winhttp {

WebSocket::WebSocket() {
    // Initialize WinHTTP session: 
    // User-Agent: Wirekrak/1.0
    // Access type: Default Proxy -> Use system proxy settings
    // No proxy name or bypass
    // No special flags
    hSession_ = WinHttpOpen(L"Wirekrak/1.0",
                            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                            WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS,
                            0);
}

WebSocket::~WebSocket() {
    close();
    if (hSession_) {
        WinHttpCloseHandle(hSession_);
    }
}

bool WebSocket::connect(const std::string& host, const std::string& port, const std::string& path)
{
    if (!hSession_) return false;

    std::wstring host_w = to_wide(host);
    std::wstring port_w = to_wide(port);
    std::wstring path_w = to_wide(path);

    hConnect_ = WinHttpConnect(hSession_, host_w.c_str(), std::stoi(port_w), 0);
    if (!hConnect_) return false;

    hRequest_ = WinHttpOpenRequest(
        hConnect_,
        L"GET",
        path_w.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );

    if (!hRequest_) return false;

    BOOL ok = WinHttpSetOption(hRequest_, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0);
    if (!ok) return false;

    ok = WinHttpSendRequest(hRequest_, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0);
    if (!ok) return false;

    ok = WinHttpReceiveResponse(hRequest_, nullptr);
    if (!ok) return false;

    // FIXED: second argument must be DWORD_PTR
    hWebSocket_ = WinHttpWebSocketCompleteUpgrade(hRequest_, (DWORD_PTR)0);
    if (!hWebSocket_) return false;

    running_ = true;
    recv_thread_ = std::thread(&WebSocket::receive_loop, this);

    return true;
}

bool WebSocket::send(const std::string& msg)
{
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

// litle description of the receive loop
// runs in a separate thread and continuously reads messages
void WebSocket::receive_loop()
{
    std::vector<char> buffer(WINHTTP_WEB_SOCKET_BUFFER_MAX_SIZE);

    while (running_) {
        DWORD bytes = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE type;

        DWORD result = WinHttpWebSocketReceive(
            hWebSocket_,
            buffer.data(),
            (DWORD)buffer.size(),
            &bytes,
            &type
        );

        if (result != ERROR_SUCCESS)
            break;

        if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
            break;

        if (callback_)
            callback_(std::string(buffer.data(), bytes));
    }

    running_ = false;
}

void WebSocket::close()
{
    if (!hWebSocket_)
        return;

    WinHttpWebSocketClose(hWebSocket_, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);

    running_ = false;

    if (recv_thread_.joinable())
        recv_thread_.join();

    WinHttpCloseHandle(hWebSocket_);
    hWebSocket_ = NULL;

    if (hRequest_)  {
        WinHttpCloseHandle(hRequest_);
        hRequest_ = NULL;
    }
    if (hConnect_)  {
        WinHttpCloseHandle(hConnect_);
        hConnect_ = NULL;
    }
}

} // namespace winhttp
} // namespace wirekrak
