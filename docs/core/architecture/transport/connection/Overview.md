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

---

## Responsibilities

- Establish and manage a **logical WebSocket connection**
- Dispatch raw text frames to higher-level protocol layers
- Track transport liveness using:
  - last received message timestamp
  - last received heartbeat timestamp
- Automatically reconnect with bounded exponential backoff
- Expose explicit hooks for:
  - message reception
  - connection establishment
  - disconnection
  - liveness timeout

---

## Liveness & Reconnection Model <a name="liveness"></a>

The connection tracks **two independent liveness signals**:

1. Last received message timestamp  
2. Last received heartbeat timestamp  

A reconnection is triggered **only if both signals are stale**, providing a conservative and
robust health check.

When liveness expires:

- The underlying transport is force-closed
- State transitions to `WaitingReconnect`
- Reconnection attempts begin with bounded exponential backoff
- A fresh transport instance is created on each attempt
- Higher-level protocol sessions are responsible for replaying subscriptions

➡️ **[Connection Liveness on Wirekrak](./Liveness.md)**

---

## State Machine

The transport connection operates using an explicit, deterministic state machine:

- `Disconnected`
- `Connecting`
- `Connected`
- `ForcedDisconnection`
- `WaitingReconnect`
- `Disconnecting`

All state transitions are logged and observable, making behavior predictable and debuggable.

---

## Usage Pattern

```cpp
wirekrak::core::transport::telemetry::Connection telemetry;
wirekrak::core::transport::Connection<WS> conn(telemetry);

conn.on_message([](std::string_view msg) {
    // forward raw frames to the protocol layer
});

conn.on_connect([]() {
    // protocol session may (re)subscribe here
});

conn.on_disconnect([]() {
    // handle disconnection if needed
});

conn.open("wss://ws.kraken.com/v2");

while (running) {
    conn.poll();
}
```

---

## Design Notes

- URL parsing is intentionally minimal (`ws://` and `wss://` only)
- TLS handling is delegated to the transport layer (e.g. WinHTTP + SChannel)
- Transport instances are **destroyed and recreated** on reconnect to avoid
  undefined internal state
- All logic is driven explicitly via `poll()`

---

⬅️ [Back to README](../../../ARCHITECTURE.md#transport-connection)
