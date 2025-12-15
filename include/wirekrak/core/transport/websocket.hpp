#pragma once

#include <string>
#include <functional>


namespace wirekrak {
namespace transport {

template<class WS>
concept WebSocket =
    requires(
        WS ws,
        std::string host,
        std::string port,
        std::string path,
        std::string msg,
        std::function<void(const std::string&)> cb
    )
    {
        // Connect must return bool
        { ws.connect(host, port, path) } -> std::same_as<bool>;

        // Must be able to send strings
        { ws.send(msg) };

        // Must be able to close connection
        { ws.close() };

        // Must accept a message callback
        { ws.set_message_callback(cb) };
    };

} // namespace transport
} // namespace wirekrak
