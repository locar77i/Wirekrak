# Wirekrak Transport Connection

## Overview

The **Wirekrak Transport Connection** is a transport-agnostic, poll-driven WebSocket
connection abstraction responsible for managing **transport lifecycle, progress,
liveness enforcement, and automatic reconnection**, independently of any exchange protocol.

A `Connection` represents a **logical transport connection** whose identity remains stable
across transient transport failures and internal reconnections, while still exposing
*observable transport facts* to higher layers.

It is the foundational component beneath protocol sessions (e.g. Kraken) and is intentionally
decoupled from message schemas, subscriptions, and business logic.

Unlike callback-driven designs, Wirekrak exposes **explicit, pull-based facts and edge-triggered
signals** that make transport behavior observable, deterministic, and suitable for
ultra-low-latency (ULL) environments.

---

## Key Characteristics

- **Header-only, zero-cost abstraction**
- **Concept-based design** (no inheritance, no virtual functions)
- **Transport-agnostic** (any WebSocket backend implementing `WebSocketConcept`)
- **Deterministic, poll-driven execution**
- **No background threads or timers**
- **ULL-safe** (no blocking, no allocations on hot paths)
- **Fully unit-testable with mock transports**

---

## Responsibilities

- Establish and manage a **logical WebSocket connection**
- Own the complete **transport lifecycle** (connect, disconnect, retry)
- Dispatch raw text frames upward
- Track **transport progress signals**
- Enforce liveness deterministically
- Automatically reconnect with bounded exponential backoff
- Emit **explicit, edge-triggered signals**
- Remain silent about protocol intent (no subscriptions, no replay)

---

## Transport Progress Contract

The Connection exposes **facts**, not inferred health states.

### Transport Epoch

- `epoch()` is a **monotonic counter**
- Incremented **exactly once per successful WebSocket connection**
- Represents completed transport lifetimes
- Never increments on retries, attempts, or failures

This allows consumers to detect:

- Fresh connections
- Transport recycling
- Stale observations

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

Lifecycle changes are surfaced via a **single-shot signal stream**.

- Signals are emitted synchronously during `poll()`
- Signals are buffered in a bounded, lock-free SPSC ring
- Signal emission never blocks
- Oldest signals may be dropped if the buffer overflows

Typical signals include:

- `Connected`
- `Disconnected`
- `RetryScheduled`
- `LivenessThreatened`

Signals are retrieved explicitly:

```cpp
connection::Signal sig;
while (conn.poll_signal(sig)) {
    // handle edge-triggered transition
}
```

Signals represent **observable consequences**, not internal states.

---

## Liveness & Reconnection Model

The connection tracks **two independent activity signals**:

1. Last received message timestamp  
2. Last received heartbeat timestamp  

A liveness failure occurs **only if both signals are stale**.

When liveness expires:

- The underlying transport is force-closed
- A `LivenessThreatened` signal may be emitted first
- A `Disconnected` signal is emitted
- State transitions toward reconnection
- Reconnection attempts begin with bounded exponential backoff
- A fresh transport instance is created on each attempt

Higher-level protocol sessions decide what (if anything) to replay.

---

## State Machine

The transport connection operates using an explicit, deterministic state machine:

- `Connecting`
- `Connected`
- `WaitingReconnect`
- `Disconnecting`
- `Disconnected`

State transitions:

- Are deterministic and logged
- Are externally observable via `connection::Signal`
- Never rely on timers, threads, or callbacks

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
        // react to edge-triggered transitions
    }
}
```

---

## Destructor Semantics

- The destructor **always closes the underlying transport**
- No retries are scheduled during destruction
- No signals are emitted after object lifetime ends
- Side effects after destruction are intentionally **not observable**

This guarantees safety, predictability, and testability.

---

## Design Notes

- URL parsing is intentionally minimal (`ws://` and `wss://` only)
- TLS handling is delegated to the transport layer
- Transport instances are **destroyed and recreated** on reconnect
- All logic is driven explicitly via `poll()`
- The Connection never invents traffic or protocol intent

---

⬅️ [Back to README](../../../ARCHITECTURE.md#transport-connection)
