# `channel::Manager` — Architecture & Contract

## Overview

`channel::Manager` is a **protocol-level state machine** responsible for tracking the lifecycle of channel subscriptions for a *single Kraken channel* (e.g. `trade`, `book`).

It models **only observable protocol facts** derived from outbound requests and inbound acknowledgements (ACKs).
It does **not** infer state, retry logic, transport health, or timing behavior.

The Manager is designed to be:
- deterministic
- replay-safe
- allocation-bounded
- ultra-low-latency (ULL) friendly
- fully testable in isolation

---

## Responsibilities

The Manager is responsible for:

- Tracking outbound **subscribe** and **unsubscribe** requests
- Tracking **pending protocol intent** awaiting ACKs
- Maintaining the authoritative set of **active subscriptions**
- Applying ACKs and rejections **exactly once**
- Exposing **queryable protocol facts** to higher layers (Session)

The Manager is **not** responsible for:

- Transport lifecycle or reconnection
- Replay scheduling or ordering
- JSON parsing or schema validation
- Timing, retries, or backoff
- Message dispatch or callbacks

---

## Scope & Ownership

Each `Manager` instance is bound to **exactly one channel**, supplied at construction:

```cpp
Manager trade_mgr{Channel::Trade};
Manager book_mgr{Channel::Book};
```

This channel identity is:
- immutable
- used only for logging and invariants
- **not** used for routing or filtering ACKs

Routing is handled externally by the Session.

---

## State Model

The Manager maintains three internal sets:

```
pending_subscriptions_    : req_id → [symbols]
pending_unsubscriptions_  : req_id → [symbols]
active_symbols_           : {symbols}
```

### State transitions

```
(initial state)
    ↓ register_subscription()
pending_subscriptions_
    ↓ subscribe ACK (success)
active_symbols_
    ↓ register_unsubscription()
pending_unsubscriptions_
    ↓ unsubscribe ACK (success)
active_symbols_ (symbol removed)
```

### Key properties

- A symbol becomes **active only after a successful ACK**
- Pending state is grouped by `req_id`, not symbol
- Rejections remove pending intent but **never** mutate active state

---

## Replay Semantics

On reconnect, **only active subscriptions are replayed**.

This is achieved implicitly:
- Replay logic re-emits subscribe requests for `active_symbols_`
- Replay ACKs flow through the **same paths** as normal ACKs
- The Manager does not distinguish replay vs user intent

This guarantees:
- deterministic convergence
- no duplicated state
- no special-case replay handling

---

## ACK & Rejection Handling

### Subscribe ACK

```cpp
process_subscribe_ack(req_id, symbol, success)
```

- `success == true`
  - Symbol moves from `pending_subscriptions_` → `active_symbols_`
- `success == false`
  - Symbol removed from `pending_subscriptions_`
- Unknown `req_id` or symbol → logged and ignored

### Unsubscribe ACK

```cpp
process_unsubscribe_ack(req_id, symbol, success)
```

- `success == true`
  - Symbol removed from `active_symbols_`
- `success == false`
  - Symbol removed from `pending_unsubscriptions_`
- Unsubscribing a non-active symbol is tolerated and logged

### Rejections

```cpp
try_process_rejection(req_id, symbol)
```

- Removes matching symbol from pending state
- Never mutates `active_symbols_`
- Safe to call for any rejection notice

---

## Public Protocol Facts

The Manager exposes **only factual state**, never inferred meaning.

### Request-level (grouped by `req_id`)

```cpp
pending_subscription_requests()
pending_unsubscription_requests()
pending_requests()
has_pending_requests()
```

### Symbol-level

```cpp
pending_subscribe_symbols()
pending_unsubscribe_symbols()
pending_symbols()
active_symbols()
has_active_symbols()
```

These queries are:
- O(1) or bounded O(n)
- allocation-free
- stable under replay and retries

---

## Reset Semantics

```cpp
clear_all()
```

Performs a full reset:
- Clears all pending intent
- Clears all active subscriptions

Used for:
- shutdown
- full protocol reset

---

## Invariants & Guarantees

### Guaranteed

- `active_symbols_` is mutated **only** on successful ACKs or reset
- Rejections never affect active state
- A symbol cannot become active without an ACK
- Duplicate or out-of-order ACKs do not corrupt state
- The Manager is replay-safe and idempotent

### Assumed (enforced by Session)

- A symbol is not subscribed and unsubscribed concurrently
- ACKs are routed to the correct Manager
- Requests use unique `req_id`s

---

## Performance Characteristics

- No dynamic allocation beyond bounded vectors
- No dynamic dispatch
- No callbacks
- No locks
- O(1) fast paths
- Channel identity is stored to avoid redundant parameters and branches

This makes the Manager suitable for **ULL, poll-driven event loops**.

---

## Design Rationale

- **Protocol facts over inferred state**
- **Composition over inheritance**
- **Replay as normal protocol flow**
- **Explicit state, minimal surface**
- **No hidden coupling to transport or timing**

---

⬅️ [Back to README](../../../ARCHITECTURE.md#channel-manager)
