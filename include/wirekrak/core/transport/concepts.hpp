#pragma once

#include <string>
#include <string_view>
#include <concepts>
#include <functional>

#include "wirekrak/core/transport/error.hpp"
#include "wirekrak/core/transport/websocket/events.hpp"

namespace wirekrak::core::transport {

// -----------------------------------------------------------------------------
// WebSocketConcept
// -----------------------------------------------------------------------------
//
// Defines the minimal contract required by the Connection layer.
//
// The WebSocket implementation:
//
//   • Owns its IO thread
//   • Pushes control-plane events into an internal SPSC ring
//   • Exposes poll_event() for the Connection to drain
//   • Temporarily retains message callback (to be replaced later)
//
// -----------------------------------------------------------------------------

template<class WS>
concept WebSocketConcept =
    requires(
        WS ws,
        const std::string& host,
        const std::string& port,
        const std::string& path,
        const std::string& msg,
        std::function<void(std::string_view msg)> on_message,
        websocket::Event ev
    )
{
    // ---------------------------------------------------------------------
    // Lifecycle
    // ---------------------------------------------------------------------

    { ws.connect(host, port, path) } noexcept -> std::same_as<Error>;
    { ws.close() } noexcept -> std::same_as<void>;

    // ---------------------------------------------------------------------
    // Sending
    // ---------------------------------------------------------------------

    { ws.send(msg) } noexcept -> std::same_as<bool>;

    // ---------------------------------------------------------------------
    // Message signaling
    // ---------------------------------------------------------------------
    { ws.set_message_callback(on_message) } -> std::same_as<void>;

    // ---------------------------------------------------------------------
    // Control-plane polling
    // ---------------------------------------------------------------------

    { ws.poll_event(ev) } noexcept -> std::same_as<bool>;
};

} // namespace wirekrak::core::transport
