#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <concepts>

#include "wirekrak/core/transport/error.hpp"


namespace wirekrak::core {
namespace transport {

// -------------------------------------------------------------------------
// WebSocketConcept defines the generic WebSocket transport contract
// required by streaming clients, independent of the underlying platform
// or implementation.
// -------------------------------------------------------------------------

template<class WS>
concept WebSocketConcept =
    requires(
        WS ws,
        const std::string& host,
        const std::string& port,
        const std::string& path,
        const std::string& msg,
        std::function<void(std::string_view msg)> on_message,
        std::function<void()> on_close,
        std::function<void(Error)> on_error
    )
{
    // Connection lifecycle
    { ws.connect(host, port, path) } -> std::same_as<Error>;
    { ws.close() } -> std::same_as<void>;

    // Sending
    { ws.send(msg) } -> std::same_as<bool>;

    // Failure & message signaling
    { ws.set_message_callback(on_message) } -> std::same_as<void>;
    { ws.set_close_callback(on_close) } -> std::same_as<void>;
    { ws.set_error_callback(on_error) } -> std::same_as<void>;
};

} // namespace transport
} // namespace wirekrak::core
