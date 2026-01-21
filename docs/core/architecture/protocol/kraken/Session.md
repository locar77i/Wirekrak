# Wirekrak Kraken Session

## Overview

The **Wirekrak Kraken Session** implements the Kraken WebSocket API v2 on top of Wirekrak’s
generic streaming infrastructure. It provides a **protocol-oriented, type-safe interface** for
subscribing to Kraken channels while abstracting away transport, reconnection, and liveness concerns.

The session is designed for low-latency systems and deterministic event processing, making it suitable
for SDKs, trading engines, and real-time market data pipelines.

---

## Design Principles

- **Composition over inheritance**
- **Zero runtime polymorphism**
- **Compile-time safety via C++20 concepts**
- **Explicit, poll-driven execution model**
- **Strict separation of concerns**

The Kraken session intentionally exposes **protocol-level concepts** only (subscriptions, acks,
typed messages) and does not leak transport or stream mechanics to the user.

---

## Architecture

The session is composed of several layers:

- **Transport layer**
  - WebSocket backend (e.g. WinHTTP, mockable)
- **Stream layer**
  - Connection lifecycle
  - Heartbeat & liveness tracking
  - Automatic reconnection
- **Protocol layer (Kraken)**
  - Request serialization
  - Message parsing and routing
  - Typed domain models
  - Channel-specific subscription management

This layered design allows the same streaming core to be reused across protocols while keeping Kraken
logic fully isolated.

---

## Core Responsibilities

- Connect and maintain a Kraken WebSocket v2 session
- Parse and route raw JSON messages into typed protocol events
- Manage subscriptions with explicit ACK tracking
- Replay active subscriptions after reconnect
- Dispatch channel messages to user callbacks
- Surface protocol-level events (status, pong, rejection)

---

## Subscription Model

Subscriptions follow an **ACK-based model**:

1. User issues a typed subscribe request
2. Request is assigned a deterministic `req_id`
3. Subscription manager tracks pending acknowledgements
4. On reconnect, subscriptions are automatically replayed
5. Callbacks are reattached transparently

This guarantees consistent behavior across reconnects and prevents accidental desynchronization.

---

## Event Processing

All events are processed via an explicit `poll()` call:

- Stream liveness and reconnection
- System messages (pong, status, rejection)
- Channel data (trades, book updates)
- Subscribe / unsubscribe acknowledgements

No background threads or hidden execution paths are used.

---

## Usage Example

```cpp
wirekrak::core::protocol::kraken::Session<wirekrak::transport::winhttp::WebSocket> session;

session.on_status([](const status::Update& s) {
    std::cout << s.str() << std::endl;
});

session.connect("wss://ws.kraken.com/v2");

session.subscribe(
    book::Subscribe{ .symbols = {"BTC/USD"}, .depth = 10, .snapshot = true },
    [](const book::Response& book) {
        // process book update
    }
);

while (running) {
    session.poll();
}
```

---

## Conclusion

This session demonstrates how to build a **robust, reusable protocol SDK** on top of a generic streaming core, while maintaining strong typing,
deterministic behavior, and clean separation between infrastructure and business logic.

---

⬅️ [Back to README](../../../ARCHITECTURE.md#kraken-session)
