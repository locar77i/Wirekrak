# Wirekrak Transport Connection

## Overview

The **Wirekrak Transport Connection** is a transport-agnostic, poll-driven WebSocket
connection abstraction responsible for managing **connection lifecycle, liveness detection,
and automatic reconnection**, independently of any exchange protocol.

A `Connection` represents a **logical transport connection** whose identity remains stable
across transient transport failures and internal reconnections.

It is the foundational component beneath protocol sessions (e.g. Kraken) and is intentionally
decoupled from message schemas, subscriptions, and business logic.

---

## Key Characteristics

- **Header-only, zero-cost abstraction**
- **Concept-based design** (no inheritance, no virtual functions)
- **Transport-agnostic** (any WebSocket backend implementing `WebSocketConcept`)
- **Deterministic, poll-driven execution**
- **No background threads or timers**
- **Fully unit-testable with mock transports**
- **ULL-safe** (no blocking, no dynamic allocation on hot paths)

---

## Responsibilities

- Establish and manage a **logical WebSocket connection**
- Dispatch raw text frames to higher-level protocol layers
- Track transport liveness using:
  - last received message timestamp
  - last received heartbeat timestamp
- Automatically reconnect with bounded exponential backoff
- Emit **explicit, semantic connection transition events**
- Remain silent about protocol intent (no subscription or message replay)

---

## Event-Driven Lifecycle Model

The transport connection does **not** expose lifecycle callbacks.

Instead, it emits **discrete transition events** that can be polled explicitly by the user.

This design:

- Eliminates hidden control flow
- Makes lifecycle observation deterministic
- Is safe for ultra-low-latency (ULL) environments
- Avoids callback reentrancy and destructor-time ambiguity

### Transition Events

The connection emits `TransitionEvent` values, such as:

- `Connected`
- `Disconnected`
- `RetryScheduled`
- `None` (no transition occurred)

Events are queued internally and retrieved via `poll_event()`.

No event emission ever blocks.  
If the internal queue is full, the **oldest event is dropped and logged**.

---

## Liveness & Reconnection Model

The connection tracks **two independent liveness signals**:

1. Last received message timestamp  
2. Last received heartbeat timestamp  

A reconnection is triggered **only if both signals are stale**, providing a conservative and
robust health check.

When liveness expires:

- The underlying transport is force-closed
- The logical connection becomes unusable
- State transitions toward reconnection
- Reconnection attempts begin with bounded exponential backoff
- A fresh transport instance is created on each attempt

Higher-level protocol sessions are responsible for deciding **what to replay** (if anything).

---

## State Machine

The transport connection operates using an explicit, deterministic state machine:

- `Connecting`
- `Connected`
- `WaitingReconnect`
- `Disconnecting`
- `Disconnected`

State transitions are **observable via transition events**, not callbacks.

A `Disconnected` event represents that the **logical connection became unusable**,
not merely that the final terminal state was reached.

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
        switch (ev) {
        case TransitionEvent::Connected:
            // connection established
            break;

        case TransitionEvent::Disconnected:
            // logical connection became unusable
            break;

        case TransitionEvent::RetryScheduled:
            // retry cycle started
            break;

        default:
            break;
        }
    }
}
```

---

## Destructor Semantics

- The destructor **always closes the underlying transport**
- No retries are scheduled after destruction
- No transition events are observable after object lifetime ends
- Destructor side effects are **not observable by design**

This guarantees deterministic teardown and avoids use-after-destruction ambiguity.

---

## Design Notes

- URL parsing is intentionally minimal (`ws://` and `wss://` only)
- TLS handling is delegated to the transport layer (e.g. WinHTTP + SChannel)
- Transport instances are **destroyed and recreated** on reconnect
- All progress is driven explicitly via `poll()`
- The connection never invents traffic, retries, or protocol intent

---

⬅️ [Back to README](../../ARCHITECTURE.md#transport-connection)
