# Wirekrak Transport WebSocket Connection

## Overview

The **Wirekrak Transport WebSocket Connection** is a deterministic,
transport-agnostic, poll-driven abstraction responsible for managing the
full lifecycle of a logical WebSocket connection.

A `Connection<WS>` instance represents a *stable logical identity*
across transient transport failures and automatic reconnections. While
the underlying WebSocket instance may be destroyed and recreated
multiple times, the `Connection` object remains stable and exposes
monotonic progress signals.

This component is intentionally protocol-agnostic and reusable across
exchanges (Kraken, future venues, custom feeds).

---

## Architectural Properties

-   Header-only
-   Concept-based design (`WebSocketConcept`)
-   No inheritance
-   No virtual dispatch
-   No background threads inside `Connection`
-   No timers
-   Fully poll-driven
-   Deterministic state transitions
-   Unit-testable with mock transports
-   Ultra-low-latency friendly

---

## Threading Contract

`Connection` is **NOT thread-safe**.

All public methods must be called from the same thread:

-   `open()`
-   `close()`
-   `send()`
-   `poll()`
-   `peek_message()`
-   `release_message()`
-   `poll_signal()`

Cross-thread interaction is isolated to the underlying WebSocket
transport, which communicates with `Connection` exclusively via bounded
SPSC rings.

All internal state within `Connection` is single-threaded and non-atomic
by design.

---

## Logical vs Physical Connection

The transport may reconnect multiple times internally.

Each successful WebSocket establishment:

-   Creates a fresh `WS` instance
-   Increments `epoch()` exactly once
-   Emits `connection::Signal::Connected`

This guarantees:

-   Epoch monotonicity
-   Stable logical identity
-   Deterministic lifetime boundaries

---

## Progress & Observability Model

The connection exposes **facts**, not inferred health states.

### Epoch

``` cpp
uint64_t epoch() const noexcept;
```

Incremented exactly once per successful connection.

Used by higher layers to detect transport recycling.

### Counters

-   `rx_messages()`
-   `tx_messages()`
-   `heartbeat_total()`

All counters are monotonic and never reset.

Counters are updated only from the user thread when progress is observed
(e.g., during `peek_message()` or `send()`).

### Signals

Edge-triggered, single-shot events delivered via an internal SPSC ring:

-   `Connected`
-   `Disconnected`
-   `RetryImmediate`
-   `RetryScheduled`
-   `LivenessThreatened`

Signals are informational only and must not be used for correctness.

Correctness must rely on epoch + counters.

---

## State Machine

Internal states:

-   `Disconnected`
-   `Connecting`
-   `Connected`
-   `Disconnecting`
-   `WaitingReconnect`

Transitions are driven exclusively through `transition_()`.

State is not externally exposed beyond `get_state()`.

---

## Connection Lifecycle

### open(url)

Preconditions:

-   Allowed only in `Disconnected` or `WaitingReconnect`
-   URL must parse successfully

Behavior:

1.  Transition to `Connecting`
2.  Create new transport
3.  Attempt connect
4.  On success → `Connected`
5.  On failure → retry cycle may begin

Calling `open()` while in `WaitingReconnect` cancels the retry cycle and
immediately attempts a fresh connection.

---

### close()

-   Idempotent
-   Cancels retry cycle
-   Transitions to `Disconnecting`
-   Emits `Disconnected` once transport closes

Destructor calls `close()`.

No retries occur after destruction.

---

## Poll Model

All progress is driven by:

``` cpp
conn.poll();
```

Responsibilities:

1.  Drain WebSocket control events
2.  Process transport errors and closures
3.  Execute reconnection attempts if retry timer expired
4.  Evaluate liveness
5.  Emit edge-triggered signals

No background activity exists outside `poll()`.

---

## Data-Plane Model

Incoming messages are delivered via a bounded SPSC ring owned by the
WebSocket transport.

``` cpp
websocket::DataBlock* block = conn.peek_message();
```

When a message is observed:

-   `rx_messages_` increments
-   `last_message_ts_` updates

The caller must release the slot explicitly:

``` cpp
conn.release_message();
```

This model guarantees:

-   Zero-copy delivery
-   Deterministic liveness updates
-   Explicit ownership boundaries

---

## Liveness Model

Two independent signals:

-   Last message timestamp
-   Last heartbeat timestamp

Liveness expires only if **both** exceed their configured timeouts.

### Warning Phase

When remaining time falls below the configured danger window,
`LivenessThreatened` is emitted once per silence window.

### Expiration Phase

When both signals are stale:

-   Transport is force-closed
-   FSM transitions to retry path
-   Immediate reconnect attempt occurs

---

## Retry Model

Retry is permitted only for transient errors:

-   `ConnectionFailed`
-   `HandshakeFailed`
-   `Timeout`
-   `RemoteClosed`
-   `TransportFailure`

Retry behavior:

1.  First retry is immediate (`RetryImmediate`)
2.  Subsequent retries use exponential backoff
3.  Backoff capped per error category
4.  Fresh transport instance created each attempt

Backoff is deterministic and bounded.

---

## Signal Buffering

Signals are stored in:

    lcr::lockfree::spsc_ring<connection::Signal, 16>

Properties:

-   Lock-free
-   Single producer
-   Single consumer
-   Allocation-free
-   Bounded capacity

Overflow policy:

-   Log warning
-   Force connection close
-   Preserve correctness guarantees

---

## Idle Detection

``` cpp
bool is_idle() const noexcept;
```

Returns true if:

-   No pending signals
-   No reconnect timer ready to fire
-   No internal work pending

Does not call `poll()`.

---

## Transport Recreation

Each reconnect:

-   Closes old transport
-   Destroys instance
-   Constructs new `WS`
-   Re-registers message callback

No transport reuse occurs.

---

## Determinism Guarantees

-   No implicit reconnection outside `poll()`
-   No hidden timers
-   No hidden threads inside `Connection`
-   No callback-based state mutation
-   All transitions logged
-   All retries explicit
-   Epoch strictly monotonic

---

## Usage Example

``` cpp
transport::telemetry::Connection telemetry;
transport::Connection<MyWebSocket> conn(telemetry);

conn.open("wss://example.com/ws");

while (running) {
    conn.poll();

    connection::Signal sig;
    while (conn.poll_signal(sig)) {
        // Optional reaction
    }

    while (auto* block = conn.peek_message()) {
        // Process message
        conn.release_message();
    }
}
```

---

## Design Philosophy

The Connection never:

-   Infers protocol intent
-   Replays subscriptions
-   Invents traffic
-   Hides transport recycling

It exposes deterministic transport facts and leaves protocol logic to
higher layers.

---

⬅️ [Back to README](../../ARCHITECTURE.md#transport-connection)
