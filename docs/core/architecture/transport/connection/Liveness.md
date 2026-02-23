
# Connection Liveness in Wirekrak

This document defines **what liveness means in Wirekrak**, how it is enforced,
and how responsibility is intentionally split between the **Connection layer**
and **Protocol layers**.

It reflects the current **epoch + counters + connection::Signal** transport
model and intentionally avoids any level-based “health state” exposure.

---

## What is liveness?

**Liveness** answers a single question:

> *Is this connection observably alive from the application’s point of view?*

Wirekrak defines liveness **strictly** and **conservatively**:

> A connection is considered alive only if observable traffic is flowing.

“Observable” means:
- A message is received from the server, and
- That traffic reaches the **Connection layer**

Silence is never assumed to be healthy.

---

## Liveness is enforced, not exposed as state

Wirekrak does **not** expose a level-based liveness or health state.

Instead, it exposes **facts**:

- Transport progress via `epoch()`
- Activity via monotonic `rx_messages()` / `tx_messages()`
- Enforcement consequences via `connection::Signal`

Liveness itself is an internal invariant, enforced deterministically by the
Connection and surfaced only through observable consequences.

---

## Why liveness is enforced

Many WebSocket APIs behave like this:

- TCP connection remains open
- TLS session remains valid
- No frames are sent
- No close frame is issued
- The socket appears “connected forever”

From an application perspective, this is indistinguishable from a dead feed.

Wirekrak refuses to guess.

Instead, it enforces a simple invariant:

> If no observable traffic arrives within the configured timeout, the connection is unhealthy.

This guarantees:
- No silent stalls
- No zombie connections
- Deterministic recovery behavior
- Predictable telemetry

---

## Liveness model (current implementation)

Wirekrak tracks a single activity signal:

| Signal | Meaning |
|--------|---------|
| Last message timestamp | When any message was received from the server |

A liveness failure occurs when:

(now - last_message_ts) > message_timeout

There is no separate heartbeat tracking at the transport layer.

If a protocol requires heartbeats or pings, they are treated simply as messages
once received. The transport layer remains protocol-agnostic.

---

## Liveness warning (pre-enforcement signal)

Before enforcement occurs, the Connection may emit:

connection::Signal::LivenessThreatened

This indicates:

> The connection is approaching enforced liveness timeout.

Characteristics:

- Emitted once per silence window
- Does not change connection state
- Is purely informational
- Represents imminent enforcement

The warning provides protocols with a last opportunity to emit traffic
(e.g., send a ping).

The Connection never sends traffic itself.
It only signals imminent enforcement.

---

## What triggers liveness enforcement?

Enforcement occurs when:

(now - last_message_ts) > message_timeout

When this happens:

1. The Connection enforces liveness failure
2. A timeout is recorded in telemetry
3. The WebSocket is force-closed
4. A connection::Signal::Disconnected edge is emitted
5. Normal reconnection logic takes over
6. A new epoch is established on successful reconnect

A successful reconnection resets the liveness baseline by anchoring
last_message_ts to the connection establishment time.

This is not an error.
It is a health enforcement action.

---

## Forced disconnection is intentional

When liveness expires, Wirekrak performs a forced disconnection.

This is deliberate:

- It uses the same closure path as real failures
- It guarantees exactly-once disconnect semantics
- It reuses the existing retry state machine
- It produces consistent telemetry

There is no special-case reconnect.

---

## Separation of responsibilities

### Connection layer responsibilities

The Connection layer:

- Enforces liveness invariants
- Detects silence deterministically
- Emits edge-triggered warning and disconnect signals
- Advances the transport epoch on successful connections
- Tracks transport progress via counters
- Provides retry, backoff, and telemetry
- Never sends protocol messages on its own

The Connection layer does not know:
- Exchange semantics
- Message formats
- Ping payloads
- Subscription rules

---

### Protocol layer responsibilities

The Protocol layer:

- Knows whether silence is acceptable
- Knows how to keep a connection alive
- Decides whether to act on a liveness warning
- Emits protocol-specific heartbeats or pings
- Decides when traffic should exist

If a protocol requires periodic pings:

The protocol must send them.

Wirekrak does not fabricate traffic.

---

## Passive connections are not safe

Opening a WebSocket without traffic is never considered safe.

If traffic stops:
- A liveness warning may be emitted
- Liveness enforcement occurs
- The transport is recycled
- A new epoch is established on reconnection

This behavior is correct and intentional.

---

## Telemetry guarantees

Wirekrak guarantees:

- Liveness warnings are observable
- Liveness timeouts are counted explicitly
- Forced reconnects are observable
- Retry attempts are measured
- Close events are exactly-once
- Errors are never double-counted

Nothing is hidden.

---

## Design philosophy

Wirekrak follows one core rule:

Correctness over convenience.

This means:
- No guessing
- No silent recovery
- No magic behavior
- Clear responsibility boundaries

If something disconnects, you can always answer:
- Why
- When
- Because of what signal

---

## Summary

- Liveness is enforced, not inferred
- Silence is unhealthy
- Warnings precede enforcement
- Forced reconnects are intentional
- Protocols must emit traffic
- The Connection remains protocol-agnostic
- Observability is non-negotiable

Wirekrak does not keep connections alive.

Protocols do.
