# Wirekrak Stream Client

## Overview

The **Wirekrak Stream Client** is a transport-agnostic, poll-driven WebSocket client designed to manage
connection lifecycle, liveness detection, and reconnection logic independently from any exchange protocol.

It is the foundational component beneath protocol clients (e.g. Kraken) and is intentionally decoupled
from message schemas and business logic.

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

- Establish and manage WebSocket connections
- Dispatch raw text frames to higher-level protocol clients
- Track connection liveness using:
  - last message timestamp
  - last heartbeat timestamp
- Automatically reconnect with bounded exponential backoff
- Expose explicit hooks for:
  - message reception
  - connection
  - disconnection
  - liveness timeout

---

## Liveness & Reconnection Model

The client tracks **two independent liveness signals**:

1. Last received message timestamp
2. Last received heartbeat timestamp

A reconnection is triggered **only if both signals are stale**, providing a conservative and robust
health check.

When liveness expires:

- The transport is force-closed
- State transitions to `WaitingReconnect`
- Reconnection attempts begin with exponential backoff
- A fresh transport instance is created on each attempt
- Higher-level protocol clients are responsible for replaying subscriptions

---

## State Machine

The stream client operates using a small explicit state machine:

- `Disconnected`
- `Connecting`
- `Connected`
- `ForcedDisconnection`
- `WaitingReconnect`
- `Disconnecting`

All state transitions are logged and observable, making behavior deterministic and debuggable.

---

## Usage Pattern

```cpp
wirekrak::stream::Client<Transport> client;

client.on_message([](std::string_view msg) {
    // forward to protocol layer
});

client.on_connect([]() {
    // subscribe to channels
});

client.on_disconnect([]() {
    // handle disconnect
});

client.connect("wss://ws.kraken.com/v2");

while (running) {
    client.poll();
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

⬅️ [Back to README](../../../README.md#stream-client)
