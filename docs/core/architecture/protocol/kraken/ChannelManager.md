# Channel Manager - Architecture & Contract

## Overview

The `channel::Manager` is a **protocol-level deterministic state machine**
responsible for tracking the lifecycle of subscriptions for a single
Kraken channel (e.g. `trade`, `book`).

It models **only observable protocol facts** derived from:

-   outbound requests
-   inbound ACKs
-   inbound rejection notices

It does **not** model transport, replay orchestration, retries,
timeouts, or business logic.

The Manager is designed to be:

-   Deterministic
-   Replay-safe
-   Idempotent
-   Allocation-bounded
-   Ultra-low-latency friendly
-   Fully unit-testable in isolation

------------------------------------------------------------------------

## Core Contract

The Manager represents **protocol truth**, not user intent.

It guarantees:

1.  A symbol becomes active **only after a successful subscribe ACK**
2.  A symbol is removed from active **only after a successful
    unsubscribe ACK**
3.  Rejections mutate only pending state
4.  Duplicate ACKs or unknown `req_id`s are safe and ignored
5.  State remains consistent under replay and reconnect storms

------------------------------------------------------------------------

## Responsibilities

The Manager is responsible for:

-   Tracking outbound subscribe/unsubscribe requests
-   Tracking pending protocol intent awaiting ACK
-   Maintaining the authoritative active symbol set
-   Applying ACKs and rejection notices exactly once
-   Exposing factual state to the Session layer

The Manager is NOT responsible for:

-   Transport lifecycle
-   Reconnect logic
-   Replay scheduling
-   JSON parsing
-   Retries or backoff
-   Callback dispatch

------------------------------------------------------------------------

## Scope & Ownership

Each instance manages exactly one channel:

    Manager trade_mgr{Channel::Trade};
    Manager book_mgr{Channel::Book};

The channel identity:

-   Is immutable
-   Is used for logging and diagnostics only
-   Is not used for routing (routing is handled by Session)

------------------------------------------------------------------------

## Internal State Model

The Manager maintains three internal structures:

    pending_subscriptions_    : req_id → [SymbolId]
    pending_unsubscriptions_  : req_id → [SymbolId]
    active_symbols_           : {SymbolId}

### Idempotency Layer

The Manager enforces symbol-level idempotency:

-   A symbol cannot exist in active twice
-   A symbol cannot be pending-subscribe twice
-   A symbol cannot be pending-unsubscribe twice

Duplicate user requests are tolerated and safely absorbed.

------------------------------------------------------------------------

## State Transitions

Initial state:

    ∅

Subscribe flow:

    register_subscription()
        → pending_subscriptions_
    subscribe ACK (success)
        → move symbol to active_symbols_
    subscribe ACK (failure)
        → remove from pending_subscriptions_

Unsubscribe flow:

    register_unsubscription()
        → pending_unsubscriptions_
    unsubscribe ACK (success)
        → remove symbol from active_symbols_
    unsubscribe ACK (failure)
        → remove from pending_unsubscriptions_

Rejections:

    try_process_rejection()
        → remove from pending only
        → never mutates active_symbols_

------------------------------------------------------------------------

## Replay Semantics

Replay uses normal protocol flow.

On reconnect:

-   Session replays active symbols
-   Replay ACKs go through normal ACK paths
-   No special-case replay logic exists inside Manager

This guarantees:

-   Deterministic convergence
-   No replay duplication
-   Idempotent behavior under reconnect storms

------------------------------------------------------------------------

## Public Protocol Facts

### Request-Level

    pending_subscription_requests()
    pending_unsubscription_requests()
    pending_requests()
    has_pending_requests()

### Symbol-Level

    pending_subscribe_symbols()
    pending_unsubscribe_symbols()
    pending_symbols()
    active_symbols()
    has_active_symbols()

### Intent-Level (Architectural Invariant)

    total_symbols()

This represents:

    active_symbols + pending_subscribe_symbols

It excludes pending_unsubscribe symbols because they are still logically
active.

------------------------------------------------------------------------

## Reset Semantics

    clear_all()

Clears:

-   Active symbols
-   Pending subscriptions
-   Pending unsubscriptions

Used only for shutdown or full protocol reset.

------------------------------------------------------------------------

## Invariants

Guaranteed:

-   Active state changes only on successful ACK
-   Rejections never mutate active state
-   Duplicate ACKs are safe
-   Unknown req_id is safe
-   Replay is idempotent
-   Symbol accounting is consistent

Assumed (enforced by Session):

-   Unique req_id generation
-   Proper ACK routing
-   Policy decisions (STRICT / LENIENT subscribe policy)

------------------------------------------------------------------------

## Performance Characteristics

-   No locks
-   No dynamic polymorphism
-   Bounded allocation
-   O(1) hot-path operations
-   Designed for poll-driven ULL event loops

------------------------------------------------------------------------

## Design Philosophy

-   Protocol facts over inferred state
-   Replay as normal protocol flow
-   Explicit state transitions
-   Idempotency at symbol granularity
-   Separation of policy (Session) and mechanics (Manager)

---

⬅️ [Back to README](../../../ARCHITECTURE.md#channel-manager)
