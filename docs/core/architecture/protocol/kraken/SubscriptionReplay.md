# Replay Module Documentation

## Overview

The **Replay Module** is designed to support resilient WebSocket-style subscription handling in a lightweight SDK client.
Its primary responsibility is to **record subscription requests** and **replay them automatically on reconnection**,
ensuring continuity after transient network failures.

This module is particularly well-suited for real-time APIs (such as Kraken WebSocket feeds) where subscriptions must be
re-established after reconnecting.

---

## Goals

- Persist subscription intent independently of connection state
- Enable deterministic replay of subscriptions
- Decouple reconnection logic from business logic
- Provide a lightweight, embeddable abstraction

---

## Module Structure

```
replay/
├── subscription.hpp   # Represents a single subscription request
├── table.hpp          # Container for active subscriptions
└── database.hpp       # High-level replay registry
```

---

## Core Components

### Subscription

A `subscription` encapsulates:
- The subscription message (typically JSON or structured payload)
- A unique key or identifier
- Optional callback or handler metadata

**Responsibilities:**
- Act as a value-type representation of a subscribe request
- Be safely replayable multiple times

---

### Table

The `table` acts as an indexed container of subscriptions.

**Responsibilities:**
- Insert and remove subscriptions
- Prevent duplicate registrations
- Iterate deterministically for replay

**Design Notes:**
- Typically implemented as a map or hash table
- Keys should be stable across reconnects

---

### Database

The `database` is the orchestration layer.

**Responsibilities:**
- Own one or more tables
- Provide a single replay entry point
- Coordinate subscription lifecycle

**Typical Flow:**
1. Client subscribes → database records subscription
2. Connection drops → database retains state
3. Connection restores → database replays all subscriptions

---

## Replay Strategy

Replay is triggered by **transport progress**, not callbacks.

On each successful WebSocket connection, the transport advances its
monotonic `epoch`. When a new epoch is observed, the Session replays
previously acknowledged subscriptions:

```text
on Connected signal with epoch > 1:
    replay_database.replay_all(send_fn)
```

Where `send_fn` is a callable that sends raw messages over the socket.

This keeps the original intent, removes callbacks, and aligns exactly
with your **epoch-driven, poll-based architecture**.

## Hackathon Pitch Angle

> “The Replay Module guarantees continuity in real-time SDKs by making subscriptions **connection-independent**.
> Developers write subscription logic once — the SDK takes care of reconnects.”

This aligns well with **Kraken Forge’s focus on building robust tools beneath the surface**.

---

## Summary

The Replay Module guarantees continuity in real-time SDKs by making subscriptions **connection-independent**.
Developers write subscription logic once — the SDK takes care of reconnects.

The Replay Module is a strong foundation for resilient real-time clients.
With minor extensions around state, policy, and observability, it can evolve into a
production-grade subscription recovery system while remaining lightweight.

---

⬅️ [Back to README](../../../ARCHITECTURE.md#subscription-replay)

