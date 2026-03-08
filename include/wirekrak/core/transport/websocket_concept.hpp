#pragma once

/*
===============================================================================
WebSocketConcept (Pull-Based, Zero-Copy)
===============================================================================

Defines the minimal transport contract required by Connection.

The WebSocket implementation:

  • Owns its receive thread
  • Writes complete messages into an internal SPSC ring
  • Exposes pull-based access to the message ring
  • Pushes control-plane events (Close / Error) into an SPSC ring
  • Is fully lifecycle-managed by Connection

No callbacks.
No message copying.
No dynamic dispatch.

-------------------------------------------------------------------------------
Threading Model
-------------------------------------------------------------------------------

Producer thread:
  - WebSocket receive thread
  - Writes message slots
  - Commits producer slot

Consumer thread:
  - Connection::poll() caller thread
  - Peeks message slot
  - Releases slot

Single-producer / single-consumer only.

===============================================================================
*/

#include <string>
#include <string_view>
#include <concepts>

#include "wirekrak/core/transport/error.hpp"


namespace wirekrak::core::transport {

template<class WS>
concept WebSocketConcept =
    requires(
        WS& ws,
        std::string_view host,
        std::uint16_t port,
        std::string_view path,
        bool secure,
        std::string_view msg
    )
{
    // ---------------------------------------------------------------------
    // Lifecycle
    // ---------------------------------------------------------------------

    { ws.connect(host, port, path, secure) } noexcept -> std::same_as<Error>;
    { ws.close() } noexcept -> std::same_as<void>;

    // ---------------------------------------------------------------------
    // Sending
    // ---------------------------------------------------------------------

    { ws.send(msg) } noexcept -> std::same_as<bool>;
};

} // namespace wirekrak::core::transport
