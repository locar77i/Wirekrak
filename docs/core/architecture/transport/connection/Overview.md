# Wirekrak Transport Connection

## Overview

The **Wirekrak Transport Connection** is a transport-agnostic, poll-driven WebSocket
connection abstraction responsible for managing **transport lifecycle, progress,
liveness enforcement, and automatic reconnection**, independently of any exchange protocol.

A `Connection` represents a **logical transport connection** whose identity remains stable
across transient transport failures and internal reconnections, while exposing only
**observable transport facts** to higher layers.

It is the foundational component beneath protocol sessions (e.g. Kraken) and is intentionally
decoupled from message schemas, subscriptions, replay logic, and business intent.

Unlike callback-driven designs, Wirekrak exposes **explicit, pull-based facts and
edge-triggered signals**, making transport behavior deterministic, observable, and
safe for ultra-low-latency (ULL) environments.

---

## Key Characteristics

- **Header-only, zero-cost abstraction**
- **Concept-based design** (no inheritance, no virtual functions)
- **Transport-agnostic** (any WebSocket backend implementing `WebSocketConcept`)
- **Deterministic, poll-driven execution**
- **No background threads, timers, or callbacks**
- **ULL-safe** (no blocking, no allocations on hot paths)
- **Fully unit-testable with mock transports**

---

## Responsibilities

- Establish and manage a **logical WebSocket connection**
- Own the complete **transport lifecycle** (connect, disconnect, retry)
- Dispatch raw text frames upward
- Track **transport progress facts**
- Enforce liveness deterministically
- Automatically reconnect using a **two-phase retry strategy**
- Emit **explicit, edge-triggered signals**
- Remain silent about protocol intent (no subscriptions, no replay)

---

## Transport Progress Contract

The Connection exposes **facts**, not inferred health or readiness states.
Correctness never depends on observing signals.

### Transport Epoch

- `epoch()` is a **monotonic counter**
- Incremented **exactly once per successful WebSocket connection**
- Represents completed transport lifetimes
- Never increments on retries, attempts, or failures

This allows consumers to detect:

- Fresh connections
- Transport recycling
- Stale observations

without relying on callbacks.

### Message Counters

- `rx_messages()` — total messages successfully received
- `tx_messages()` — total messages successfully sent

Both counters are:

- Monotonic
- Never reset
- Updated only on successful operations

Together with `epoch()`, these counters form the **transport progress contract**.

---

## Event Model (`connection::Signal`)

Externally observable transitions are surfaced via a **single-shot signal stream**.

- Signals are emitted synchronously during `poll()`
- Signals are buffered in a bounded, lock-free SPSC ring
- Signal emission never blocks and never allocates
- Oldest signals may be dropped if the buffer overflows

Typical signals include:

- `Connected`
- `Disconnected`
- `RetryImmediate`
- `RetryScheduled`
- `LivenessThreatened`

Signals are retrieved explicitly:

```cpp
connection::Signal sig;
while (conn.poll_signal(sig)) {
    // handle edge-triggered consequence
}
```

Signals represent **observable consequences**, not internal state or intent.

---

## Liveness & Reconnection Model

The connection tracks **two independent activity signals**:

1. Last received message timestamp  
2. Last received heartbeat timestamp  

A liveness failure occurs **only if both signals are stale**.

### Liveness Expiry

When liveness expires:

- The underlying transport is force-closed
- A `LivenessThreatened` signal may have been emitted earlier
- A `Disconnected` signal is emitted exactly once
- Reconnection logic takes over deterministically

---

## Retry Semantics (ULL-safe)

Reconnection follows a **strict two-phase model**:

### 1. Immediate Retry

- The **first reconnect attempt is always immediate**
- No delay, no backoff, no signal scheduling
- Represented by a `RetryImmediate` signal (optional observation)
- Executed synchronously inside `poll()`

### 2. Scheduled Retry (Backoff)

- Applied **only if the immediate reconnect fails**
- Exponential backoff is enforced deterministically
- A `RetryScheduled` signal is emitted
- Subsequent reconnect attempts occur only after backoff expiry

There are:

- No retry callbacks
- No user hooks
- No implicit timers

All retry behavior is observable via signals and progress facts only.

---

## State Machine

Internally, the Connection uses an explicit, deterministic state machine:

- `Connecting`
- `Connected`
- `WaitingReconnect`
- `Disconnecting`
- `Disconnected`

Internal state is **not exposed**.

Only externally meaningful facts and signals are observable.

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
        // new transport lifetime observed
        last_epoch = conn.epoch();
    }

    connection::Signal sig;
    while (conn.poll_signal(sig)) {
        // optional observation
    }
}
```

---

## Destructor Semantics

- The destructor **always closes the underlying transport**
- No retries are scheduled after destruction
- No signals are emitted after object lifetime ends
- Destructor side effects are intentionally **not observable**

This guarantees safety, predictability, and testability.

---

## Design Notes

- URL parsing is intentionally minimal (`ws://` and `wss://` only)
- TLS handling is delegated to the transport layer
- Transport instances are **destroyed and recreated** on reconnect
- All logic is driven explicitly via `poll()`
- The Connection never invents traffic, retries, or protocol behavior

---

⬅️ [Back to README](../../../ARCHITECTURE.md#transport-connection)
