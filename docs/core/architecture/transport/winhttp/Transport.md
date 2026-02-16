
# WinHTTP WebSocket Transport

This document describes the **WinHTTP-based WebSocket transport** used by Wirekrak
as the production Windows transport backend.

The implementation lives in:

```
wirekrak::core::transport::winhttp::WebSocketImpl<Api>
```

It provides a **policy-free, single-connection WebSocket primitive**
used by `core::transport::Connection`.

---

## Design Philosophy

The WinHTTP transport is intentionally minimal and deterministic.

It is responsible only for:

- Establishing a WebSocket over WinHTTP
- Sending raw WebSocket frames
- Receiving frames on a dedicated receive thread
- Translating OS-level errors into semantic `transport::Error`
- Publishing control-plane events (Close / Error) via SPSC queue

It does **NOT**:

- Perform reconnection
- Implement backoff
- Track liveness
- Interpret protocol messages
- Maintain logical session state

All policy decisions are handled by `core::transport::Connection`.

---

## Architectural Layering

```
core::transport::Connection
        ▲
        │ WebSocketConcept
        │
transport::winhttp::WebSocketImpl<RealApi>
        ▲
        │ ApiConcept
        │
transport::winhttp::RealApi (WinHTTP)
```

The transport is a thin OS adapter.

---

## Threading Model

The transport uses **one dedicated receive thread**.

- `connect()` starts `receive_loop_()`
- `close()` stops the loop and joins the thread
- `send()` is synchronous (caller thread)

### Control Events (SPSC)

Control-plane events are published through:

```
lcr::lockfree::spsc_ring<websocket::Event, 16>
```

Producer:
- Receive thread only

Consumer:
- `core::transport::Connection::poll()`

This guarantees:
- Single producer
- Single consumer
- No locks
- Deterministic event ordering

Only two event types exist:

- `Event::Close`
- `Event::Error(Error)`

---

## Lifecycle

### connect(host, port, path)

1. Creates WinHTTP session
2. Performs HTTP upgrade
3. Completes WebSocket handshake
4. Spawns receive thread
5. Returns `Error::None` on success

No reconnection is attempted at this level.

---

### send(message)

- Synchronously calls `WinHttpWebSocketSend`
- Returns `true` if accepted by OS
- Returns `false` if not connected or send failed

Errors are surfaced asynchronously via control events.

---

### close()

- Idempotent
- Signals receive loop to stop
- Closes WinHTTP handles
- Joins receive thread
- Guarantees exactly one Close event is emitted

---

## Receive Loop

`receive_loop_()`:

- Blocks on `WinHttpWebSocketReceive`
- Handles:
  - Complete messages
  - Fragmented messages
  - Close frames
  - WinHTTP errors
- Emits control events via SPSC ring
- Ensures `signal_close_()` runs exactly once

Message delivery occurs via registered callback:

```
set_message_callback(std::function<void(std::string_view)>)
```

This callback runs on the receive thread.

---

## Error Handling

WinHTTP error codes are translated into semantic `transport::Error`:

| WinHTTP Code | transport::Error |
|--------------|------------------|
| OPERATION_CANCELLED | LocalShutdown |
| CONNECTION_ERROR | RemoteClosed |
| TIMEOUT | Timeout |
| CANNOT_CONNECT | ConnectionFailed |
| Other | TransportFailure |

The transport **classifies**, but never retries.

---

## TLS & Security

- TLS handled by Windows SChannel
- System certificate validation
- No OpenSSL dependency
- No external networking libraries

---

## Determinism Guarantees

- No virtual functions
- No dynamic polymorphism
- No retry policy
- No hidden timers
- Close event emitted exactly once
- Error surfaced exactly once per failure

All behavior is explicit and poll-driven at the Connection layer.

---

## Testing Strategy

The transport is templated on `ApiConcept`:

```
template<ApiConcept Api = RealApi>
class WebSocketImpl;
```

This allows:

- Injection of fake WinHTTP APIs
- Deterministic error simulation
- No real network dependency
- Full unit test coverage

---

## Summary

The WinHTTP WebSocket transport is:

- A deterministic OS adapter
- Single-connection
- Failure-signaling only
- Thread-safe via SPSC event queue
- Fully testable via API abstraction

All connection lifecycle management lives above this layer.

---

⬅️ [Back to Transport Overview](../Overview.md#winhttp)
