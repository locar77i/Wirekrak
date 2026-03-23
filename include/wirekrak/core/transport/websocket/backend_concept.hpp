#pragma once

/*
================================================================================
WebSocket Backend Concept (Transport-Agnostic)
================================================================================

This concept defines the minimal interface required by the Wirekrak WebSocket
transport layer to interact with any underlying backend (Asio, WinHTTP, etc).

Design goals:
  • Backend-agnostic — no dependency on Asio, WinHTTP, or OS-specific types
  • Zero-copy friendly — caller provides buffer, backend writes directly
  • Streaming-oriented — supports partial reads and message fragmentation
  • Deterministic semantics — transport drives the loop, backend reports state
  • Testability — enables fake backends for unit testing without network/OS

The transport (WebSocketImpl) owns:
  • Receive loop
  • Backpressure handling
  • Message assembly
  • Telemetry
  • Error propagation

The backend (adapter) owns:
  • Socket / OS interaction
  • Protocol framing (WebSocket)
  • Mapping native errors → ReceiveStatus

--------------------------------------------------------------------------------
Contract
--------------------------------------------------------------------------------

read_some():
  - Writes up to `size` bytes into `buffer`
  - Sets `bytes` to number of bytes written
  - Returns a ReceiveStatus describing the outcome
  - May return partial frames (streaming)
  - MUST satisfy:
        • If status == Ok:
              bytes ∈ [0, size]
        • If status != Ok:
              bytes MUST be 0

message_done():
  - Returns true when a full WebSocket message has been received
  - Must be consistent with read_some() boundaries

--------------------------------------------------------------------------------
Error Model
--------------------------------------------------------------------------------

The backend MUST map all backend-specific errors into ReceiveStatus.

The transport layer does NOT interpret OS-specific errors.

--------------------------------------------------------------------------------
Threading
--------------------------------------------------------------------------------

All methods are called from a single-threaded receive loop, except:
  - send() may be called concurrently depending on higher layers

Implementations must document thread-safety guarantees.

================================================================================
*/

#include <concepts>
#include <string_view>
#include <cstdint>
#include <cstddef>
#include <utility> // std::as_const

namespace wirekrak::core::transport::websocket {

// -----------------------------------------------------------------------------
// Normalized receive result (backend → transport)
// -----------------------------------------------------------------------------
enum class ReceiveStatus {
    Ok,             // Data read successfully (bytes > 0 or 0 for control frames)
    Closed,         // Graceful close (WebSocket CLOSE frame or equivalent)
    Timeout,        // Read timed out (if supported by backend)
    ProtocolError,  // WebSocket protocol violation
    TransportError  // Underlying transport/socket error
};

// -----------------------------------------------------------------------------
// WebSocket Backend Concept
// -----------------------------------------------------------------------------
template<class T>
concept BackendConcept = requires(
    T backend,
    std::string_view host,
    std::uint16_t port,
    std::string_view target,
    bool secure,
    std::string_view msg,
    void* buffer,
    std::size_t size,
    std::size_t& bytes
) {
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------
    { backend.connect(host, port, target, secure) } -> std::same_as<bool>;
    { backend.close() } -> std::same_as<void>;

    // -------------------------------------------------------------------------
    // State (must be const-correct)
    // -------------------------------------------------------------------------
    { std::as_const(backend).is_open() } -> std::same_as<bool>;

    // -------------------------------------------------------------------------
    // Send (synchronous, best-effort)
    // -------------------------------------------------------------------------
    { backend.send(msg) } -> std::same_as<bool>;

    // -------------------------------------------------------------------------
    // Receive (streaming, zero-copy)
    // -------------------------------------------------------------------------
    { backend.read_some(buffer, size, bytes) } -> std::same_as<ReceiveStatus>;

    // -------------------------------------------------------------------------
    // Message boundary (must be const-correct)
    // -------------------------------------------------------------------------
    { std::as_const(backend).message_done() } -> std::same_as<bool>;
};

} // namespace wirekrak::core::transport::websocket
