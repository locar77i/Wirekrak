#pragma once

#include <string_view>

namespace wirekrak::core {
namespace transport {

/*
===============================================================================
 transport::Error
===============================================================================

Transport-level error classification.

This enum represents *semantic transport failures*, abstracted away from
platform- or library-specific error codes (WinHTTP, ASIO, etc.).

It is intentionally:
- small
- stable
- policy-free

Higher layers (e.g. transport::Connection, protocol sessions) may use this
classification to decide whether and how to recover.
===============================================================================
*/

enum class Error {
    None = 0,

    // --- Expected / benign termination --------------------------------------

    LocalShutdown,    // Connection was closed intentionally by the local endpoint
    RemoteClosed,     // Remote endpoint closed the connection gracefully (CLOSE frame)

    // --- Transient / recoverable failures -----------------------------------

    Timeout,          // Transport-level timeout (idle, stalled network, etc.)
    ConnectionFailed, // Connection attempt failed (DNS, handshake, routing, etc.)

    // --- Protocol / framing issues ------------------------------------------

    ProtocolError,    // Invalid frame, protocol violation, or unexpected message structure

    // --- Fatal / unspecified transport failure ------------------------------
    
    TransportFailure, // Unclassified or unrecoverable transport failure
};


/// Optional helper for logging / diagnostics
inline constexpr std::string_view to_string(Error err) noexcept {
    switch (err) {
    case Error::None:              return "None";
    case Error::LocalShutdown:     return "LocalShutdown";
    case Error::RemoteClosed:      return "RemoteClosed";
    case Error::Timeout:           return "Timeout";
    case Error::ConnectionFailed:  return "ConnectionFailed";
    case Error::ProtocolError:     return "ProtocolError";
    case Error::TransportFailure:  return "TransportFailure";
    default:                       return "Unknown";
    }
}

} // namespace transport
} // namespace wirekrak::core
