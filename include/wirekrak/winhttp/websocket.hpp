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

class WebSocket {
    // TODO: Reduce buffer size until BUG (incomplete messages) appears
    constexpr static size_t WINHTTP_WEB_SOCKET_BUFFER_MAX_SIZE = 16 * 1024; // 16 KB

public:
    WebSocket();
    ~WebSocket();

    bool connect(const std::string& host, const std::string& port, const std::string& path);

    bool send(const std::string& msg);

    void close();

    void set_message_callback(std::function<void(const std::string&)> cb) {
        callback_ = std::move(cb);
    }

private:
    void receive_loop();

private:
    HINTERNET hSession_ = NULL;
    HINTERNET hConnect_ = NULL;
    HINTERNET hRequest_ = NULL;
    HINTERNET hWebSocket_ = NULL;

    std::thread recv_thread_;
    std::atomic<bool> running_ = false;

    std::function<void(const std::string&)> callback_;
};
// Defensive check that WebSocket conforms to the transport::WebSocket concept
static_assert(wirekrak::transport::WebSocket<WebSocket>);


} // namespace winhttp
} // namespace wirekrak
