/*
===============================================================================
WebSocketConcept (Pull-Based, Zero-Copy)
===============================================================================

Defines the minimal transport contract required by Connection.

The WebSocket implementation:

  • Owns its receive thread
  • Writes complete messages into an internal SPSC ring of DataBlock
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
  - Writes DataBlock
  - Commits producer slot

Consumer thread:
  - Connection::poll() caller thread
  - Peeks DataBlock
  - Releases slot

Single-producer / single-consumer only.

-------------------------------------------------------------------------------
Ownership Model
-------------------------------------------------------------------------------

DataBlock memory is owned by the WebSocket ring.

Connection / Session:

  - May read DataBlock->data[0..size)
  - Must call release_consumer_slot()
  - Must NOT retain pointer after release

===============================================================================
*/
#pragma once

#include <string>
#include <string_view>
#include <concepts>

#include "wirekrak/core/transport/error.hpp"


namespace wirekrak::core::transport {

template<class WS>
concept WebSocketConcept =
    requires(
        WS ws,
        const std::string& host,
        const std::string& port,
        const std::string& path,
        const std::string_view msg
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
};

} // namespace wirekrak::core::transport
