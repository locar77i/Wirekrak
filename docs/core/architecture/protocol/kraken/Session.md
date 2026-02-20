# Wirekrak Kraken Session

## Overview

The **Wirekrak Kraken Session** implements the Kraken WebSocket API v2
on top of Wirekrak's generic transport and streaming infrastructure.

It provides a **protocol-oriented, type-safe interface** for interacting
with Kraken channels while deliberately exposing only **observable
protocol facts**.

> **Important:** The Session expresses *protocol intent*, not
> completion. Callers submit subscribe/unsubscribe requests but never
> observe protocol identifiers or lifecycle details.

The Session is designed for **deterministic, ultra-low-latency (ULL)
systems** such as trading engines, SDKs, and real-time market data
pipelines.

---

## Architectural Properties

-   Header-only Core
-   Composition over inheritance
-   Zero runtime polymorphism
-   Compile-time safety via C++20 concepts
-   Explicit, poll-driven execution model
-   Deterministic replay semantics
-   Fact-based observability
-   Fail-fast protocol correctness
-   Strict Core / Lite separation

---

## Threading Contract

`Kraken::Session` is **NOT thread-safe**.

All public methods must be invoked from the same thread:

-   `open()` / `close()`
-   `subscribe()` / `unsubscribe()`
-   `poll()`
-   All `pop_*()` and `try_load_*()` APIs

Concurrency boundaries exist only between:

-   Transport IO thread (inside WebSocket implementation)
-   Core session thread (user thread)

Cross-thread communication is strictly isolated via bounded SPSC rings.

All protocol logic is single-threaded and deterministic.

---

## Architectural Position

The Kraken Session is a **Core-level component**.

Core responsibilities:

-   Own protocol correctness
-   Own internal request identifiers (`req_id`)
-   Own ACK tracking and replay eligibility
-   Own rejection processing
-   Own transport epoch awareness

User code must not depend on protocol correlation identifiers.

The Session never exposes internal `req_id` values.

---

## Layered Architecture

The Session composes three strictly separated layers:

### 1. Transport Layer

-   WebSocket backend (WinHTTP or mock)
-   Connection lifecycle management
-   Liveness enforcement
-   Transport epoch tracking
-   Control-plane signaling

### 2. Session Layer (Core)

-   Composition over `transport::Connection`
-   Deterministic reaction to transport signals
-   ACK-driven subscription state machine
-   Replay of acknowledged protocol intent only
-   Lock-free message exposure
-   Rejection surfacing
-   No callbacks, no symbol routing

### 3. Protocol Layer (Kraken Schema)

-   Request serialization
-   JSON parsing
-   Message classification
-   Schema validation
-   Typed domain models

This separation keeps Core allocation-stable, minimal, and ULL-friendly.

---

## Subscription Model

Subscriptions follow an **ACK-driven intent model**.

### Subscribe

``` cpp
session.subscribe(schema::trade::Subscribe{ .symbols = {"BTC/EUR"} });
```

Semantics:

1.  Internal `req_id` assigned
2.  Request serialized and sent immediately
3.  Intent enters *Pending* state
4.  On ACK success → becomes replay-eligible
5.  On rejection → permanently discarded

The assigned `req_id` is never returned and never observable.

---

### Unsubscribe

``` cpp
session.unsubscribe(schema::trade::Unsubscribe{ .symbols = {"BTC/EUR"} });
```

Semantics:

-   Intent expressed immediately
-   Replay eligibility removed deterministically
-   ACK handled internally
-   Caller must not assume immediate exchange-side effect

Unsubscribe ACK identifiers are not correlated with subscribe
identifiers. This complexity is fully encapsulated inside Core.

---

## Subscription State Model

Each subscription exists in exactly one of:

-   NotRequested
-   Pending
-   Active (ACKed)
-   Rejected (terminal)

Only **Active** subscriptions are eligible for replay.

Rejected subscriptions are final and never retried automatically.

---

## Replay Semantics

Replay occurs only when:

-   Transport epoch increments
-   A new connection is successfully established

Replay rules:

-   Only ACKed subscriptions replay
-   Order preserved
-   Idempotent
-   Deterministic
-   No replay during transient connection failure

Replay never occurs without an epoch change.

---

## Rejection Semantics

Protocol rejections are:

-   Surfaced verbatim
-   Lossless
-   Ordered
-   Authoritative
-   Never auto-retried

Rejections are placed into a bounded lock-free ring.

Failure to drain rejections is considered a user logic error.

Core will not invent corrective behavior.

---

## Pong & Status Semantics

Pong and status updates represent **state**, not streams.

-   Only latest value retained
-   Intermediate updates may be overwritten
-   No buffering beyond latest snapshot

Exposed via pull-based APIs:

``` cpp
bool try_load_pong(schema::system::Pong&);
bool try_load_status(schema::status::Update&);
```

These APIs are non-blocking and allocation-free.

---

## Data-Plane Message Access

Channel responses are exposed via lock-free rings:

``` cpp
bool pop_trade_message(schema::trade::Response&);
bool pop_book_message(schema::book::Response&);
```

Core does NOT:

-   Dispatch callbacks
-   Route by symbol
-   Partition by channel instance

All message routing belongs to Lite.

---

## Execution Model

All progress is driven explicitly via:

``` cpp
uint64_t poll();
```

Each call advances:

1.  Transport polling
2.  Connection signal handling
3.  Transport epoch detection
4.  JSON parsing
5.  Protocol routing
6.  ACK processing
7.  Replay if required
8.  Rejection buffering

There are no background threads in Core.

If `poll()` is not called, no progress occurs.

---

## Transport Epoch Integration

Session observes `connection.epoch()`.

When epoch increases:

-   All Active subscriptions replay
-   Pending subscriptions remain pending
-   Rejected subscriptions remain discarded

Epoch is the only reconnection boundary signal.

---

## Determinism Guarantees

-   No hidden retries
-   No implicit subscription repair
-   No protocol inference
-   No callback-based mutation
-   No background activity
-   No implicit replay
-   Replay strictly epoch-bounded
-   All protocol state explicit and finite

Core never guesses.

---

## Failure Model

Transport failures are handled by `Connection`.

Protocol failures are handled by Session.

Failure domains are strictly separated.

Transport failure does NOT imply protocol failure.

Protocol rejection does NOT imply transport instability.

---

## Example Usage

``` cpp
kraken::Session<MyWebSocket> session(telemetry);

session.open("wss://ws.kraken.com/v2");

session.subscribe(schema::trade::Subscribe{ .symbols = {"BTC/EUR"} });

while (running) {
    session.poll();

    schema::trade::Response trade;
    while (session.pop_trade_message(trade)) {
        // User-level routing
    }
}
```

---

## Core vs Lite

Core:

-   Deterministic protocol engine
-   Typed message exposure
-   Replay enforcement
-   Rejection surfacing

Lite:

-   Symbol routing
-   Callback registration
-   Convenience APIs
-   User-friendly ergonomics

Core remains minimal, strict, and allocation-stable.

---

## Design Philosophy

Wirekrak Core never:

-   Infers user intent
-   Repairs rejected subscriptions
-   Hides transport recycling
-   Exposes protocol identifiers
-   Dispatches callbacks
-   Owns business logic

It exposes protocol truth and enforces correctness.

---

⬅️ [Back to README](../../../ARCHITECTURE.md#kraken-session)
