#pragma once

#include <string>

namespace wirekrak::lite {

/*
===============================================================================
Lite Error Model (v1 — STABLE)
===============================================================================

Lite errors represent *semantic failures* observable by SDK users.

They intentionally abstract away Core-level and transport-specific details
to provide a stable, portable, and user-friendly error surface.

[transport] indicates a failure while establishing the connection to the remote
endpoint. This error is reported during `connect()` and means that no active
stream was created. Typical causes include network errors, TLS handshake failures,
or unreachable endpoints. No protocol or subscription state was established.

[protocol] indicates that a message received from the server violated expected
protocol or schema invariants. This may include malformed payloads, unexpected
message types, or values that cannot be mapped to strongly typed Wirekrak schemas.
Protocol errors are considered fatal for the current connection and may result
in stream termination.

[rejected] indicates that the server explicitly rejected a client request, such as
a subscription or unsubscription. This error represents a valid, well-formed server
response indicating that the requested operation was not accepted (e.g. duplicate
subscriptions, invalid symbols, or permission errors). The connection itself remains
healthy.

[disconnected] indicates that the underlying stream has entered a terminal
state and the client can no longer receive or send messages. This may be caused
by transport failures, liveness timeouts, protocol errors, or explicit server
disconnects. The exact cause is intentionally abstracted away at the Lite level.

Error codes may be extended in future versions, but existing values will
never change meaning.
===============================================================================
*/

enum class error_code {
    transport,     // Network / socket / OS failure / connect failures
    protocol,      // Invalid or unexpected protocol message
    rejected,      // Server rejected a request
    disconnected   // Connection closed or lost: stream entered terminal state
};

struct error {
    error_code code;
    std::string message; // Human-readable explanation
};

} // namespace wirekrak::lite
