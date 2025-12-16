#pragma once

#include <string>
#include <functional>
#include <concepts>

namespace wirekrak::transport {

template<class WS>
concept WebSocket =
    requires(
        WS ws,
        const std::string& host,
        const std::string& port,
        const std::string& path,
        const std::string& msg,
        std::function<void(const std::string&)> on_message,
        std::function<void()> on_close,
        std::function<void(unsigned long)> on_error
    )
{
    // Connection lifecycle
    { ws.connect(host, port, path) } -> std::same_as<bool>;
    { ws.close() } -> std::same_as<void>;

    // Sending
    { ws.send(msg) } -> std::same_as<bool>;

    // Failure & message signaling
    { ws.set_message_callback(on_message) } -> std::same_as<void>;
    { ws.set_close_callback(on_close) } -> std::same_as<void>;
    { ws.set_error_callback(on_error) } -> std::same_as<void>;
};

} // namespace wirekrak::transport
