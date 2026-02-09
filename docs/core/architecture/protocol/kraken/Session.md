# Wirekrak Kraken Session

## Overview

The **Wirekrak Kraken Session** implements the Kraken WebSocket API v2 on top of
Wirekrak’s generic transport and streaming infrastructure.

It provides a **protocol-oriented, type-safe interface** for interacting with
Kraken channels while deliberately exposing only **observable protocol facts**.
No inferred health, implicit retries, hidden dispatch, or speculative recovery
logic is ever exposed.

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

The Session exposes **Kraken protocol truth only**: raw typed messages, ACKs,
rejections, and progress facts.

All user-facing behavior (partitioning, dispatch, callbacks) lives outside Core.

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
- Managing subscriptions with explicit ACK tracking
- Replaying **only acknowledged subscriptions** after reconnect
- Exposing protocol messages via lock-free rings
- Surfacing authoritative protocol rejections
- Reacting deterministically to transport signals

The Session **never dispatches callbacks**, never partitions messages, and never
assumes user intent.

---

## Progress Contract (Session Facts)

The Session exposes **progress facts**, not connection state.

### Transport Epoch

```cpp
uint64_t transport_epoch() const;
uint64_t poll(); // returns current epoch
```

- Incremented **exactly once per successful WebSocket connection**
- Monotonic for the lifetime of the Session
- Never increments on retries or transient failures

Defines replay and staleness boundaries.

### Message Counters

```cpp
uint64_t rx_messages() const;
uint64_t tx_messages() const;
uint64_t hb_messages() const;
```

- Monotonic
- Never reset
- Updated only on successful observation or transmission

---

## Subscription Model

Subscriptions follow an **ACK-driven intent model**:

1. User submits a typed subscribe request
2. A deterministic `req_id` is assigned
3. Subscription enters a pending state
4. On ACK success, it becomes active
5. On rejection, intent is removed permanently

### Replay Semantics

- Only **acknowledged subscriptions** are replayed
- Rejected intent is **final**
- Replay occurs **only after transport epoch change**
- No replay without a successful reconnect

---

## Rejection Semantics

Protocol rejections are:

- Surfaced verbatim
- Treated as authoritative
- Lossless and ordered
- Never retried, repaired, or replayed

Rejections are processed internally for correctness, then exposed via a bounded,
lossless queue.

Failure to drain rejections is considered a **user error** and results in
fail-fast shutdown.

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

Change detection is per-calling thread.

---

## Data-Plane Message Access

Channel messages are exposed as **raw protocol responses** via lock-free rings.

```cpp
bool pop_trade_message(schema::trade::Response&);
bool pop_book_message(schema::book::Response&);

template<class F>
void drain_trade_messages(F&&);

template<class F>
void drain_book_messages(F&&);
```

Core does **not** partition or dispatch messages.
All symbol routing and callback logic belongs to Lite.

---

## Event Processing Model

All progress is driven explicitly by:

```cpp
uint64_t poll();
```

Each call advances:

- Transport liveness and reconnection
- Connection signal handling
- Protocol message routing into rings
- ACK and rejection processing

There are **no background threads, timers, or callbacks** in Core.

---

## Usage Example

```cpp
Session session;
session.connect("wss://ws.kraken.com/v2");

session.subscribe(schema::book::Subscribe{
    .symbols = {"BTC/USD"},
    .depth   = 10
});

while (running) {
    session.poll();

    session.drain_book_messages([](const schema::book::Response& msg) {
        // partition & dispatch in Lite
    });
}
```

---

## Summary

The Kraken Session provides:

- Deterministic, poll-driven execution
- Explicit protocol authority (ACKs and rejections)
- Lossless semantic error reporting
- Clear replay boundaries via epoch
- Fact-based observability
- A clean Core / Lite separation

Wirekrak Core never guesses.  
It exposes protocol truth and enforces correctness.

---

⬅️ [Back to README](../../../ARCHITECTURE.md#kraken-session)
