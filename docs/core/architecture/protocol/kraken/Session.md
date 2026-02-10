# Wirekrak Kraken Session

## Overview

The **Wirekrak Kraken Session** implements the Kraken WebSocket API v2 on top of
Wirekrak’s generic transport and streaming infrastructure.

It provides a **protocol-oriented, type-safe interface** for interacting with
Kraken channels while deliberately exposing only **observable protocol facts**.

> **Important:** The Session expresses *protocol intent*, not completion.
> Callers submit subscribe/unsubscribe requests but never observe protocol
> identifiers or lifecycle details.

The Session is designed for **deterministic, ultra-low-latency (ULL) systems**
such as trading engines, SDKs, and real-time market data pipelines.

---

## Design Principles

- **Composition over inheritance**
- **Zero runtime polymorphism**
- **Compile-time safety via C++20 concepts**
- **Explicit, poll-driven execution model**
- **Fact-based observability**
- **Fail-fast semantics for protocol violations**
- **Strict separation of Core and Lite responsibilities**

The Session exposes **Kraken protocol truth only**: typed messages, ACK effects,
rejections, and progress facts.

All user-facing behavior (dispatch, callbacks, symbol routing) lives outside Core.

---

## Architectural Position

The Kraken Session is a **Core-level component**.

- It owns protocol correctness
- It owns request identifiers (`req_id`)
- It owns replay and ACK tracking
- It never exposes protocol identifiers to callers

User code must not depend on protocol correlation mechanisms.

---

## Architecture

The Session is composed of three strictly separated layers:

### Transport Layer
- WebSocket backend (WinHTTP or mockable)
- Connection lifecycle and reconnection
- Heartbeat tracking and liveness enforcement
- Transport progress accounting

### Session Layer (Core)
- Composition over `transport::Connection`
- Deterministic reaction to transport signals
- Replay of **acknowledged protocol intent only**
- Exposure of **protocol facts via pull APIs**
- Enforcement of protocol correctness

### Protocol Layer (Kraken)
- Request serialization
- JSON parsing and routing
- Schema validation
- Channel-specific subscription management
- Typed protocol domain models

This architecture keeps Core minimal, allocation-stable, and ULL-focused.

---

## Core Responsibilities

The Kraken Session is responsible for:

- Establishing and maintaining a Kraken WebSocket session
- Parsing and routing raw JSON into typed protocol messages
- Assigning and managing internal protocol request identifiers
- Tracking subscription state with explicit ACK handling
- Replaying **only acknowledged subscriptions** after reconnect
- Surfacing authoritative protocol rejections
- Exposing protocol messages via lock-free rings

The Session **never dispatches callbacks**, never partitions messages, and never
assumes user intent.

---

## Subscription Model

Subscriptions follow an **ACK-driven intent model**.

### Subscribe

```cpp
session.subscribe(schema::trade::Subscribe{ .symbols = {"BTC/EUR"} });
```

Semantics:

- A protocol request identifier is assigned internally
- The request is sent immediately
- The subscription enters a *pending* state
- On ACK success, it becomes replay-eligible
- On rejection, the intent is discarded permanently

> The assigned `req_id` is **never returned** and **never observable**.

### Unsubscribe

```cpp
session.unsubscribe(schema::trade::Unsubscribe{ .symbols = {"BTC/EUR"} });
```

Semantics:

- Unsubscribe intent is expressed immediately
- Replay intent is removed deterministically
- Protocol ACKs are handled internally
- The caller must not assume immediate exchange-side effect

Unsubscribe ACK identifiers are **not correlated** with subscribe identifiers.
This complexity is intentionally hidden inside Core.

---

## Replay Semantics

- Only **acknowledged subscriptions** are replayed
- Rejected intent is **final**
- Replay occurs **only after transport epoch change**
- No replay without a successful reconnect

Replay is deterministic and idempotent.

---

## Rejection Semantics

Protocol rejections are:

- Surfaced verbatim
- Treated as authoritative
- Lossless and ordered
- Never retried, repaired, or replayed

Rejections are processed internally for correctness, then exposed via a bounded,
lossless queue.

Failure to drain rejections is considered a **user error**.

---

## Pong & Status Semantics

Pong and status messages are treated as **state**, not streams:

- Only the most recent value is retained
- Intermediate updates may be overwritten
- No buffering, backpressure, or callbacks

They are exposed via pull-based APIs:

```cpp
bool try_load_pong(schema::system::Pong&);
bool try_load_status(schema::status::Update&);
```

---

## Data-Plane Message Access

Channel messages are exposed as **raw protocol responses** via lock-free rings.

```cpp
bool pop_trade_message(schema::trade::Response&);
bool pop_book_message(schema::book::Response&);
```

Core does **not** partition or dispatch messages.
All symbol routing and callbacks belong to Lite.

---

## Execution Model

All progress is driven explicitly by:

```cpp
uint64_t poll();
```

Each call advances:

- Transport liveness and reconnection
- Connection signal handling
- Protocol message routing into rings
- ACK and rejection processing

There are **no background threads** in Core.

---

## Summary

The Kraken Session provides:

- Deterministic, poll-driven execution
- Internal ownership of protocol identifiers
- Explicit ACK- and rejection-driven correctness
- Replay bounded by transport epochs
- Fact-based observability
- A clean Core / Lite separation

Wirekrak Core never guesses.
It exposes protocol truth and enforces correctness.

---

⬅️ [Back to README](../../../ARCHITECTURE.md#kraken-session)
