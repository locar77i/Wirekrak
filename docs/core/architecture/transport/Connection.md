# Wirekrak Transport Connection

## Overview

The **Wirekrak Transport Connection** is a transport-agnostic, poll-driven WebSocket
connection abstraction responsible for managing **transport lifecycle, liveness enforcement,
progress signaling, and automatic reconnection**, independently of any exchange protocol.

A `Connection` represents a **logical transport connection** whose identity remains stable
across transient transport failures and internal reconnections, while still exposing
**observable progress facts** to higher layers.

It is the foundational component beneath protocol sessions (e.g. Kraken) and is intentionally
decoupled from message schemas, subscriptions, and business logic.

Unlike callback-driven designs, Wirekrak exposes **explicit, pull-based signals**
that make transport progress observable, deterministic, and suitable for ultra-low-latency
(ULL) environments.

---

## Key Characteristics

- **Header-only, zero-cost abstraction**
- **Concept-based design** (no inheritance, no virtual functions)
- **Transport-agnostic** (any WebSocket backend implementing `WebSocketConcept`)
- **Deterministic, poll-driven execution**
- **No background threads or timers**
- **Fully unit-testable with mock transports**
- **ULL-safe** (no blocking, no allocations on hot paths)

---

## Responsibilities

- Establish and manage a **logical WebSocket connection**
- Own the complete **transport lifecycle** (connect, disconnect, retry)
- Dispatch raw text frames to higher-level protocol layers
- Track **transport progress and activity**
- Enforce **liveness deterministically**
- Automatically reconnect with bounded exponential backoff
- Emit **edge-triggered transport signals**
- Remain silent about protocol intent (no subscriptions or message replay)

---

## Progress & Observability Model

The transport connection exposes **facts, not inferred health states**.
Correctness never depends on observing signals.

### Transport Epoch

- `epoch() -> uint64_t`
- Incremented **exactly once per successful WebSocket connection**
- Represents completed transport lifetimes
- Monotonic for the lifetime of the `Connection`

The epoch allows higher layers to detect **transport recycling**
without lifecycle hooks or callbacks.

### Message Counters

- `rx_messages()`
- `tx_messages()`

Monotonic counters tracking **successfully received and sent messages**.
They act as progression signals and are never reset.

### Signals (`connection::Signal`)

Edge-triggered, single-shot signals representing externally observable facts:

- `Connected`
- `Disconnected`
- `RetryScheduled`
- `LivenessThreatened`

Signals are **informational only** and never represent full state.

---

## Signal Delivery Model

- Signals are emitted synchronously during `poll()`
- Buffered internally in a bounded, lock-free SPSC ring
- Non-blocking and allocation-free
- Oldest signal is dropped if the buffer overflows
- Signals are not replayed across transport lifetimes

Users must rely on **epoch + counters** for correctness.
Signals are optional.

---

## Liveness & Reconnection Model

The connection tracks **two independent activity signals**:

1. Last received message timestamp
2. Last received heartbeat timestamp

Liveness failure occurs **only if both signals are stale**.

When liveness expires:

- The underlying transport is force-closed
- A `LivenessThreatened` signal may have been emitted earlier
- A forced disconnect occurs
- Normal reconnection logic applies
- A fresh transport instance is created on each attempt

Higher-level protocol sessions decide **what to replay**, if anything.

---

## State Machine

Internally, the transport connection uses an explicit, deterministic state machine:

- `Connecting`
- `Connected`
- `WaitingReconnect`
- `Disconnecting`
- `Disconnected`

Internal state is **not exposed**.
Only externally meaningful signals and progress facts are observable.

---

## Usage Pattern

```cpp
wirekrak::core::transport::telemetry::Connection telemetry;
wirekrak::core::transport::Connection<WS> conn(telemetry);

conn.open("wss://ws.kraken.com/v2");

uint64_t last_epoch = conn.epoch();

while (running) {
    conn.poll();

    if (conn.epoch() != last_epoch) {
        // Transport lifetime changed
        last_epoch = conn.epoch();
    }

    connection::Signal sig;
    while (conn.poll_signal(sig)) {
        // React if desired (optional)
    }
}
```

---

## Destructor Semantics

- The destructor **always closes the underlying transport**
- No retries are scheduled after destruction
- No signals are emitted after object lifetime ends
- Destructor side effects are intentionally **not observable**

This guarantees deterministic teardown and avoids use-after-destruction ambiguity.

---

## Design Notes

- URL parsing is intentionally minimal (`ws://` and `wss://` only)
- TLS handling is delegated to the transport layer
- Transport instances are **destroyed and recreated** on reconnect
- All progress is driven explicitly via `poll()`
- The connection never invents traffic or protocol behavior

---

⬅️ [Back to README](../../ARCHITECTURE.md#transport-connection)
