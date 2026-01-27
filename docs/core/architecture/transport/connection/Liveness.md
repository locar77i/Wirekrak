# Connection Liveness in Wirekrak

This document defines **what liveness means in Wirekrak**, how it is enforced,
and how responsibility is intentionally split between the **Connection layer**
and **Protocol layers**.

If you are investigating reconnects, timeouts, or “unexpected” disconnects,
this document defines the rules that govern that behavior.

---

## What is liveness?

**Liveness** answers a single question:

> *Is this connection observably alive from the application’s point of view?*

Wirekrak defines liveness **strictly** and **conservatively**:

> A connection is considered *alive* **only if observable traffic is flowing**.

“Observable” means:
- A message is received from the server, and
- That message reaches the Connection layer

Silence is never assumed to be healthy.

---

## Why liveness is enforced

Many WebSocket APIs behave like this:

- TCP connection remains open
- TLS session remains valid
- No frames are sent
- No close frame is issued
- The socket appears “connected forever”

From an application perspective, this is **indistinguishable from a dead feed**.

Wirekrak refuses to guess.

Instead, it enforces a simple invariant:

> **If nothing happens, the connection is unhealthy.**

This guarantees:
- No silent stalls
- No zombie connections
- Deterministic recovery behavior
- Predictable telemetry

---

## Liveness signals

Wirekrak tracks two independent timestamps:

| Signal | Meaning |
|------|--------|
| **Last message timestamp** | When any message was received |
| **Last heartbeat timestamp** | When a protocol heartbeat was observed |

A liveness timeout occurs **only if both signals are stale**.

This allows:
- Heartbeat-only protocols
- Data-only protocols
- Mixed traffic

But it never allows *silence*.

---

## What triggers a liveness timeout?

A timeout occurs when:

```
(now - last_message_ts) > message_timeout
AND
(now - last_heartbeat_ts) > heartbeat_timeout
```

When this happens:

1. The Connection **decides** the connection is unhealthy
2. A liveness timeout is recorded
3. The WebSocket is **force-closed**
4. Normal reconnection logic takes over

This is **not an error**.  
It is a **health enforcement action**.

---

## Forced disconnection is intentional

When liveness expires, Wirekrak performs a **forced disconnection**.

This is deliberate:

- It uses the same closure path as real failures
- It guarantees exactly-once disconnect semantics
- It reuses the existing retry state machine
- It produces consistent telemetry

There is no “special case” reconnect.

This keeps the system:
- Testable
- Observable
- Predictable

---

## Separation of responsibilities

### Connection layer responsibilities

The Connection layer:

- Enforces liveness invariants
- Detects silence deterministically
- Forces reconnects when required
- Provides retry, backoff, and telemetry
- Never sends protocol messages on its own

The Connection layer **does not know**:
- Exchange semantics
- Message formats
- Ping payloads
- Subscription rules

---

### Protocol layer responsibilities

The Protocol layer:

- Knows whether silence is acceptable
- Knows how to keep a connection alive
- Emits protocol-specific heartbeats or pings
- Decides when traffic should exist

If a protocol requires periodic pings:

> **The protocol must send them.**

Wirekrak does not fabricate traffic.

---

## Passive connections are not safe

Many exchanges allow opening a WebSocket without subscribing.

Observed behaviors include:
- Emitting a single welcome message
- Emitting nothing at all
- Emitting periodic heartbeats
- Going completely silent

Wirekrak treats *all of these equally*.

If traffic stops:
- Liveness expires
- The connection is recycled

This is correct.

---

## Telemetry guarantees

Wirekrak guarantees:

- Liveness timeouts are counted explicitly
- Forced reconnects are observable
- Retry attempts are measured
- Close events are exactly-once
- Errors are never double-counted

Nothing is hidden.

---

## Design philosophy

Wirekrak follows one core rule:

> **Correctness over convenience.**

This means:
- No guessing
- No silent recovery
- No magic behavior
- Clear responsibility boundaries

If something disconnects, you can always answer:
- **Why**
- **When**
- **Because of what signal**

---

## Summary

- Liveness is enforced, not inferred
- Silence is unhealthy
- Forced reconnects are intentional
- Protocols must emit traffic
- The Connection remains protocol-agnostic
- Observability is non-negotiable

Wirekrak does not keep connections alive.

**Protocols do.**

---

Wirekrak enforces correctness —  
it does not hide responsibility.

---

⬅️ [Back to README](./Overview.md#liveness)
