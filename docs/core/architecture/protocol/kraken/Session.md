# Wirekrak Kraken Session

## Overview

The **Wirekrak Kraken Session** implements the Kraken WebSocket API v2 on top of
Wirekrak’s generic transport and streaming infrastructure.

It provides a **protocol-oriented, type-safe interface** for interacting with
Kraken channels while deliberately exposing only **observable protocol facts**,
never inferred health, hidden retries, or implicit recovery behavior.

The session is designed for **deterministic, ultra-low-latency (ULL) systems** and
is suitable for SDKs, trading engines, and real-time market data pipelines.

---

## Design Principles

- **Composition over inheritance**
- **Zero runtime polymorphism**
- **Compile-time safety via C++20 concepts**
- **Explicit, poll-driven execution model**
- **Fact-based observability (no inferred state)**
- **Fail-fast semantics for protocol violations**
- **Strict separation of transport, session, and protocol concerns**

The Kraken Session exposes **Kraken protocol truth only** (subscriptions, ACKs,
rejections, typed messages) and never leaks transport internals or speculative
state.

---

## Architecture

The session is composed of three strictly separated layers:

### Transport Layer
- WebSocket backend (WinHTTP or mockable)
- Connection lifecycle and reconnection
- Heartbeat tracking and liveness enforcement
- Transport progress accounting

### Session Layer
- Composition over `transport::Connection`
- Deterministic reaction to transport signals
- Replay of **acknowledged protocol intent only**
- Exposure of **transport and protocol facts**
- Enforcement of protocol correctness

### Protocol Layer (Kraken)
- Request serialization
- JSON parsing and routing
- Schema validation
- Channel-specific subscription management
- Typed domain models

This architecture allows reuse of the streaming core across protocols while
keeping Kraken-specific logic fully isolated.

---

## Core Responsibilities

The Kraken Session is responsible for:

- Establishing and maintaining a Kraken WebSocket session
- Parsing and routing raw JSON into typed protocol messages
- Managing subscriptions with explicit ACK tracking
- Replaying **only acknowledged subscriptions** after reconnect
- Dispatching channel messages to user callbacks
- Exposing protocol-level facts (pong, status, rejection)
- Reacting deterministically to transport signals

The session **never invents protocol intent**, never retries rejected requests,
and never hides protocol failures.

---

## Progress Contract (Session Facts)

The Kraken Session exposes **progress facts**, not health state.

### Transport Epoch

```cpp
uint64_t transport_epoch() const;
uint64_t poll(); // returns current epoch
```

- Incremented **exactly once per successful WebSocket connection**
- Monotonic for the lifetime of the Session
- Never increments on retries or transient failures

The epoch defines **replay and staleness boundaries**.

### Message Counters

```cpp
uint64_t rx_messages() const;
uint64_t tx_messages() const;
uint64_t hb_messages() const;
```

- Monotonic counters
- Never reset
- Updated only on successful observation or transmission

Together with `transport_epoch`, these counters form the **Session progress
contract**.

---

## Connection Signals

The session observes transport-level events via
`transport::connection::Signal`:

- `Connected`
- `Disconnected`
- `RetryScheduled`
- `LivenessThreatened`

These signals represent **externally observable consequences**, not inferred
connection state.

The session reacts internally (cleanup, replay, optional ping) but does **not**
expose connection flags or callbacks.

---

## Subscription Model

Subscriptions follow an **ACK-driven intent model**:

1. User issues a typed subscribe request
2. A deterministic `req_id` is assigned
3. Subscription enters a pending state
4. On ACK success, it becomes active
5. On rejection, intent is removed permanently

### Replay Semantics

- Only **acknowledged subscriptions** are replayed
- Rejected intent is **final**
- Replay is triggered **only by transport epoch change**
- No replay occurs without a successful reconnect

This guarantees protocol correctness across reconnects.

---

## Rejection Semantics

Protocol rejections are:

- Surfaced verbatim
- Treated as authoritative
- Lossless and ordered
- Never retried
- Never repaired
- Never replayed

Rejections are processed internally for correctness and then exposed to the user
via a bounded, lossless queue. Failure to drain rejections is considered a user
error and results in a fail-fast shutdown.

---

## Pong & Status Semantics

Pong and status messages are treated as **state**, not streams:

- Only the **most recent value** is retained
- Intermediate updates may be overwritten
- No buffering or backpressure
- No callbacks or observers

They are exposed via pull-based APIs:

```cpp
bool try_load_pong(schema::system::Pong&);
bool try_load_status(schema::status::Update&);
```

This avoids reentrancy and enforces deterministic processing.

---

## Liveness Semantics

Liveness is enforced internally and never exposed as state.

- Silence is considered unhealthy
- Enforcement occurs only if both message and heartbeat signals are stale
- A `LivenessThreatened` signal may be emitted before enforcement
- Enforcement results in forced disconnect and normal reconnection

The session may optionally send protocol pings based on policy, but the transport
never fabricates traffic.

---

## Event Processing Model

All progress is driven explicitly by:

```cpp
uint64_t poll();
```

Each call advances:

- Transport liveness and reconnection
- Connection signal handling
- Protocol facts (pong, status, rejection)
- Channel messages and ACKs

There are **no background threads, timers, or callbacks** in Core.

---

## Usage Example

```cpp
Session session;
session.connect("wss://ws.kraken.com/v2");

session.subscribe(
    schema::book::Subscribe{ .symbols = {"BTC/USD"}, .depth = 10 },
    [](const schema::book::Response& book) {
        // process book update
    }
);

schema::status::Update status;
schema::rejection::Notice rejection;

while (running) {
    session.poll();

    if (session.try_load_status(status)) {
        std::cout << "STATUS: " << status << std::endl;
    }

    while (session.pop_rejection(rejection)) {
        std::cerr << "REJECTION: " << rejection << std::endl;
    }
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
- Strict separation of responsibilities

Wirekrak does not guess protocol intent.  
It exposes truth and enforces correctness.

---

⬅️ [Back to README](../../../ARCHITECTURE.md#kraken-session)
