# WinHTTP WebSocket Transport

This document describes the **WinHTTP-based WebSocket transport** used
by Wirekrak as the production Windows transport backend.

The implementation lives in:

    wirekrak::core::transport::winhttp::WebSocketImpl<MessageRing, Api>

It provides a **policy-free, single-connection WebSocket primitive**
used by `core::transport::Connection`.

---

## Design Philosophy

The WinHTTP transport is intentionally minimal, deterministic, and
**ultra‑low‑latency (ULL) oriented**.

It is responsible only for:

- Establishing a WebSocket over WinHTTP
- Sending raw WebSocket frames
- Receiving frames on a dedicated receive thread
- Translating OS-level errors into semantic `transport::Error`
- Publishing control-plane events (Close / Error) via SPSC queue
- Writing data-plane messages into an externally owned zero-copy SPSC
    ring

It does **NOT**:

- Perform reconnection
- Implement backoff
- Track liveness
- Interpret protocol messages
- Maintain logical session state
- Own message memory

All policy decisions and memory ownership live above this layer.

---

## Architectural Layering

    core::protocol::kraken::Session
            ▲
            │ owns MessageRing
            │
    core::transport::Connection
            ▲
            │ WebSocketConcept
            │
    transport::winhttp::WebSocketImpl<MessageRing, RealApi>
            ▲
            │ ApiConcept
            │
    transport::winhttp::RealApi (WinHTTP)

The transport is a thin OS adapter writing directly into externally
owned memory.

---

## Message Ring Ownership Model

The message ring is **injected into the WebSocket**:

``` cpp
template<typename MessageRing, ApiConcept Api = RealApi>
class WebSocketImpl;
```

The constructor receives a reference:

``` cpp
WebSocketImpl(MessageRing& ring, telemetry::WebSocket& telemetry);
```

### Ownership Rules

- The ring is owned by the Session layer
- The WebSocket only writes into it
- The WebSocket never allocates message memory
- The WebSocket never destroys the ring
- The ring must outlive the WebSocket

This guarantees:

- Deterministic memory layout
- No hidden allocations
- Stable memory across reconnects
- Clear destruction order

---

## Threading Model

The transport uses **one dedicated receive thread**.

- `connect()` starts `receive_loop_()`
- `close()` stops the loop and joins the thread
- `send()` is synchronous (caller thread)

### Control Events (SPSC)

Control-plane events are published through:

    lcr::lockfree::spsc_ring<websocket::Event, 16>

Producer: Receive thread\
Consumer: `core::transport::Connection::poll()`

Properties:

- Single producer
- Single consumer
- Lock-free
- Deterministic ordering

Only two event types exist:

- `Event::Close`
- `Event::Error(Error)`

---

## Data Plane: Zero-Copy Message Ring

Message delivery uses a **single-producer / single-consumer** ring:

    lcr::lockfree::spsc_ring<DataBlock, N>

### DataBlock Structure

    struct DataBlock {
        std::uint32_t size;
        char data[RX_BUFFER_SIZE];
    };

Properties:

- Fixed-size blocks
- No dynamic allocation
- No std::string buffering
- No intermediate copies
- Deterministic layout

### Producer (Receive Thread)

The receive thread:

1.  Lazily acquires a producer slot
2.  Writes fragments directly into slot memory
3.  Accumulates size across fragments
4.  Commits slot only on final frame

If backpressure occurs (ring full):

- A fatal transport error is emitted
- The connection is force-closed
- No silent data loss occurs

Strict correctness \> availability.

### Consumer (Connection Layer)

The Connection layer explicitly pulls messages:

``` cpp
while (auto* block = ws.peek_message()) {
    process(block->data, block->size);
    ws.release_message();
}
```

Guarantees:

- Zero-copy handoff
- Explicit lifetime management
- No callback execution on receive thread
- Deterministic consumption

---

## Lifecycle

### connect(host, port, path)

1.  Creates WinHTTP session
2.  Performs HTTP upgrade
3.  Completes WebSocket handshake
4.  Resets close state
5.  Spawns receive thread

No reconnection logic exists here.

---

### send(message)

- Calls `WinHttpWebSocketSend`
- Returns `true` if accepted by OS
- Errors surface asynchronously via control events

---

### close()

- Idempotent
- Signals receive loop to stop
- Cancels blocking receive
- Joins receive thread
- Closes WinHTTP handles
- Emits exactly one Close event

Uncommitted DataBlocks are safely abandoned.

---

## Receive Loop Model

`receive_loop_()`:

- Acquires ring slot lazily
- Writes fragments directly into final memory
- Guards against buffer overflow
- Commits only complete messages
- Abandons slot on error
- Emits control events
- Ensures close signaling exactly once

There are no callbacks and no hidden copies.

---

## Error Translation

| WinHTTP Code | transport::Error |
|--------------|------------------|
| OPERATION_CANCELLED | LocalShutdown |
| CONNECTION_ERROR | RemoteClosed |
| TIMEOUT | Timeout |
| CANNOT_CONNECT | ConnectionFailed |
| Other | TransportFailure |

The transport classifies but never retries.

---

## Determinism Guarantees

- No virtual dispatch
- No dynamic polymorphism
- No retry logic
- No hidden timers
- Exactly-once close signaling
- Exactly-once error signaling
- Zero-copy message delivery
- No allocation in receive path

All lifecycle policy lives above this layer.

---

## Testing Strategy

The transport is templated on `ApiConcept`:

    template<typename MessageRing, ApiConcept Api = RealApi>
    class WebSocketImpl;

This enables:

- Fake WinHTTP injection
- Deterministic error simulation
- No real network dependency
- Full unit-test coverage
- Deterministic backpressure testing

---

## Summary

The WinHTTP WebSocket transport is:

- A deterministic OS adapter
- Single-connection
- Failure-signaling only
- Memory-injection based
- Zero-copy
- Ultra-low-latency oriented
- Fully testable

All connection policy, memory ownership, and lifecycle management live
above this layer.

---

⬅️ [Back to Transport Overview](../Overview.md#winhttp)
