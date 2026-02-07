# Wirekrak Kraken Session

## Overview

The **Wirekrak Kraken Session** implements the Kraken WebSocket API v2 on top of
Wirekrak’s generic transport and streaming infrastructure.

It provides a **protocol-oriented, type-safe interface** for interacting with
Kraken channels while deliberately exposing only **observable facts and protocol
truth**, not inferred connection health or hidden recovery behavior.

The session is designed for **deterministic, low-latency (ULL) systems**, making it
suitable for SDKs, trading engines, and real-time market data pipelines.

---

## Design Principles

- **Composition over inheritance**
- **Zero runtime polymorphism**
- **Compile-time safety via C++20 concepts**
- **Explicit, poll-driven execution model**
- **Fact-based observability (no inferred state)**
- **Strict separation of transport, session, and protocol concerns**

The Kraken Session exposes **Kraken concepts only** (subscriptions, ACKs,
rejections, typed messages) and does not leak transport internals.

---

## Architecture

The session is composed of three clearly separated layers:

### Transport Layer
- WebSocket backend (WinHTTP or mockable)
- Connection lifecycle
- Automatic reconnection with backoff
- Liveness enforcement
- Transport progress tracking

### Session Layer
- Composition over `transport::Connection`
- Deterministic reaction to transport signals
- Replay of **acknowledged** protocol intent
- Exposure of **transport progress facts**

### Protocol Layer (Kraken)
- Request serialization
- JSON parsing and routing
- Schema validation
- Channel-specific subscription management
- Typed domain models

This design allows the same streaming core to be reused across protocols while
keeping Kraken logic fully isolated.

---

## Core Responsibilities

The Kraken Session is responsible for:

- Establishing and maintaining a Kraken WebSocket session
- Parsing and routing raw JSON messages into typed protocol events
- Managing subscriptions with explicit ACK tracking
- Replaying **only acknowledged subscriptions** after reconnect
- Dispatching channel messages to user callbacks
- Surfacing protocol-level facts (status, pong, rejection)
- Reacting to transport signals deterministically

The session never invents protocol intent and never repairs rejected requests.

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
- Never increments on retries, failures, or disconnects

The epoch represents **completed transport lifetimes** and defines replay and
staleness boundaries.

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

These signals represent **externally observable consequences**, not internal
state.

The session reacts internally (cleanup, replay, optional ping) but does **not**
expose connection state flags or callbacks.

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
- Replay is triggered **only by epoch change**
- No replay occurs without a successful reconnect

This guarantees protocol correctness across reconnects.

---

## Rejection Semantics

Protocol rejections are:

- Surfaced verbatim
- Treated as authoritative
- Never retried
- Never repaired
- Never replayed

Rejection handling removes intent from all managers and the replay database.

---

## Liveness Semantics

Liveness is **enforced internally**, not exposed as state.

- Silence is considered unhealthy
- Enforcement occurs only if **both message and heartbeat signals are stale**
- A `LivenessThreatened` signal may be emitted before enforcement
- Enforcement results in forced disconnect and normal reconnection

The session may optionally react (e.g. send ping) based on policy, but the
transport never fabricates traffic.

---

## Event Processing Model

All progress is driven explicitly by:

```cpp
uint64_t poll();
```

Each call advances:

- Transport liveness and reconnection
- Connection signal handling
- System messages (pong, status, rejection)
- Channel messages and ACKs

There are **no background threads, timers, or callbacks**.

---

## Usage Example

```cpp
using Session = wirekrak::core::protocol::kraken::Session<
    wirekrak::core::transport::winhttp::WebSocket
>;

Session session;

session.on_status([](const schema::status::Update& s) {
    std::cout << s << std::endl;
});

session.connect("wss://ws.kraken.com/v2");

session.subscribe(
    schema::book::Subscribe{ .symbols = {"BTC/USD"}, .depth = 10 },
    [](const schema::book::Response& book) {
        // process book update
    }
);

uint64_t last_epoch = session.transport_epoch();

while (running) {
    uint64_t epoch = session.poll();
    if (epoch != last_epoch) {
        // transport recycled
        last_epoch = epoch;
    }
}
```

---

## Summary

The Kraken Session provides:

- Deterministic, poll-driven execution
- Clear protocol authority (ACKs and rejections)
- Explicit replay boundaries via epoch
- Fact-based observability
- Strict separation of responsibilities

Wirekrak does not guess protocol intent.  
It exposes truth and enforces correctness.

---

⬅️ [Back to README](../../../ARCHITECTURE.md#kraken-session)
