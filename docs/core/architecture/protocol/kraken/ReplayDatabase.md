# Replay Database - Architecture & Contract

## Overview

The `replay::Database` is a deterministic protocol-intent store used by
the Session to guarantee subscription continuity across transport
reconnects.

It stores **acknowledged user intent**, but is always mutated by
**server truth**: - Explicit rejections - Successful unsubscriptions -
Strict symbol idempotency rules

ReplayDB is:

-   Protocol-facing (not business-facing)
-   Deterministic
-   Idempotent
-   Replay-safe
-   Transport-agnostic
-   Allocation-stable after warm-up

It does **not** perform I/O and does not infer protocol state.

---

## Core Contract

ReplayDB stores:

> "What the user intends to be subscribed --- as confirmed by the
> exchange --- minus anything the server explicitly rejected."

This contract has three fundamental rules:

1.  **Accepted intent persists**
2.  **Rejected intent is removed immediately**
3.  **Silence preserves intent** (network lapse does not imply
    rejection)

This guarantees deterministic recovery after reconnect.

---

## Architectural Structure

    replay/
    ├── subscription.hpp   # Symbol-granular subscription container
    ├── table.hpp          # Per-channel intent table (idempotent)
    └── database.hpp       # Multi-channel replay registry

Each channel (trade, book, etc.) has its own typed `Table<RequestT>`.

---

## Design Philosophy

ReplayDB models **protocol facts only**, not assumptions.

It does NOT: - Retry requests - Repair intent - Track callbacks - Track
timing - Infer server state

It simply stores *validated subscription intent*.

Replay happens as a normal protocol flow.

---

## Symbol-Granular Idempotency

ReplayDB enforces **strict symbol uniqueness** per channel.

Internally, each `Table<RequestT>` maintains:

    subscriptions_ : req_id → Subscription<RequestT>
    symbol_owner_  : symbol_id → req_id

### Policy: FIRST-WRITE-WINS

When adding a new subscription request:

-   Any symbol already owned by another request is dropped
-   Existing subscriptions are never mutated
-   Empty requests are discarded

This guarantees:

-   No duplicated symbols
-   Deterministic ownership
-   Replay stability
-   Bounded state growth

---

## Mutation by Server Truth

ReplayDB is mutated only by:

### 1) Successful unsubscribe ACK

Removes symbol from table and ownership map.

### 2) Rejection notice

Removes symbol from matching request. If the request becomes empty → it
is erased.

### 3) Explicit erase_symbol()

Used to apply unsubscribe semantics matching Kraken behavior.

ReplayDB never mutates active intent speculatively.

---

## Replay Semantics

On reconnect:

    take_subscriptions()
    → Session resends stored requests
    → Normal ACK flow applies

Replay uses the same protocol path as user-driven requests.

ReplayDB does not distinguish replay from fresh intent.

This guarantees convergence.

---

## Determinism Guarantees

ReplayDB guarantees:

-   Symbol ownership is unique
-   Total symbol count == ownership map size
-   No duplicated replay
-   Idempotent behavior under reconnect storms
-   Stable state under delayed ACKs

Debug builds provide:

    assert_consistency()

Ensuring internal map invariants remain valid.

---

## Performance Characteristics

-   O(1) symbol ownership checks
-   O(1) symbol erase
-   No dynamic dispatch
-   No locks
-   No callbacks
-   No blocking

Suitable for ultra-low-latency event loops.

---

## Lifecycle

### add(req)

Stores acknowledged subscription intent (idempotent).

### try_process_rejection(req_id, symbol)

Applies permanent server rejection.

### erase_symbol(symbol)

Applies unsubscribe semantics.

### take_subscriptions()

Transfers intent for replay.

### clear()

Drops all stored intent (shutdown).

---

## System-Level Role

ReplayDB is the **intent backbone** of the Session.

The Manager tracks *protocol lifecycle*. ReplayDB tracks *long-term
subscription intent*.

Together they guarantee:

-   Crash-safe subscription continuity
-   Deterministic convergence
-   Isolation between channels
-   Replay storm resilience

---

⬅️ [Back to README](../../../ARCHITECTURE.md#replay-database)

