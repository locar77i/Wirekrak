# Wirekrak Transport Connection

## Overview

The **Wirekrak Transport Connection** is a transport-agnostic, poll-driven WebSocket
connection abstraction responsible for managing **connection lifecycle, liveness detection,
and automatic reconnection**, independently of any exchange protocol.

A `Connection` represents a **logical transport connection** whose identity remains stable
across transient transport failures and internal reconnections.

It is the foundational component beneath protocol sessions (e.g. Kraken) and is intentionally
decoupled from message schemas, subscriptions, and business logic.

Unlike callback-driven designs, Wirekrak exposes **explicit, pull-based transition events**
that make lifecycle changes observable, deterministic, and suitable for ultra-low-latency
(ULL) environments.

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
- Dispatch raw text frames to higher-level protocol layers
- Track transport liveness using:
  - last received message timestamp
  - last received heartbeat timestamp
- Automatically reconnect with bounded exponential backoff
- Emit **explicit connection transition events** (no lifecycle hooks)

---

## Event Model (TransitionEvent)

Connection lifecycle changes are surfaced via a **single-shot event stream** consumed
explicitly by the user.

- Events are emitted synchronously during `poll()` execution
- Events are buffered internally using a lock-free SPSC ring
- Events are never blocking
- Events are never inferred or synthesized

Typical events include:

- `Connected`
- `Disconnected`
- `RetryScheduled`
- `None` (no transition)

The consumer retrieves events using:

```cpp
TransitionEvent ev;
while (conn.poll_event(ev)) {
    // handle transition
}
```

This replaces traditional `on_connect`, `on_disconnect`, and `on_retry` hooks.

---

## Liveness & Reconnection Model <a name="liveness"></a>

The connection tracks **two independent liveness signals**:

1. Last received message timestamp  
2. Last received heartbeat timestamp  

A reconnection is triggered **only if both signals are stale**, providing a conservative and
robust health check.

When liveness expires:

- The underlying transport is force-closed
- A `Disconnected` transition event is emitted
- State transitions to `WaitingReconnect`
- Reconnection attempts begin with bounded exponential backoff
- A fresh transport instance is created on each attempt
- Higher-level protocol sessions decide what (if anything) to replay

➡️ **[Connection Liveness on Wirekrak](./Liveness.md)**

---

## State Machine

The transport connection operates using an explicit, deterministic state machine:

- `Connecting`
- `Connected`
- `WaitingReconnect`
- `Disconnecting`
- `Disconnected`

State transitions:

- Are fully deterministic
- Are logged
- Are externally observable via `TransitionEvent`
- Never depend on timing, background threads, or implicit callbacks

---

## Usage Pattern

```cpp
wirekrak::core::transport::telemetry::Connection telemetry;
wirekrak::core::transport::Connection<WS> conn(telemetry);

conn.open("wss://ws.kraken.com/v2");

while (running) {
    conn.poll();

    TransitionEvent ev;
    while (conn.poll_event(ev)) {
        // react explicitly to lifecycle changes
    }
}
```

---

## Destructor Semantics

- The destructor **always closes the underlying transport**
- No retries are scheduled during destruction
- No transition events are emitted after object lifetime ends
- Side effects after destruction are intentionally **not observable**

This guarantees safety, predictability, and testability.

---

## Design Notes

- URL parsing is intentionally minimal (`ws://` and `wss://` only)
- TLS handling is delegated to the transport layer (e.g. WinHTTP + SChannel)
- Transport instances are **destroyed and recreated** on reconnect to avoid
  undefined internal state
- All logic is driven explicitly via `poll()`
- The Connection never guesses intent and never repairs protocol-level failures

---

⬅️ [Back to README](../../../ARCHITECTURE.md#transport-connection)

