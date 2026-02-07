# Kraken Session Liveness Enforcement

This example demonstrates **connection liveness enforcement in Wirekrak**
using the `protocol::kraken::Session`.

It shows how **transport-level liveness invariants are enforced by
`transport::Connection`**, while **responsibility for producing observable
liveness signals is delegated to the protocol layer** through an explicit
policy.

The Session does not guess, infer, or hide behavior — it reacts
**deterministically** to observable signals emitted by the Connection.

---

## Contract Demonstrated

- Liveness invariants are enforced at the transport layer
- The protocol layer decides **whether and how** to satisfy them
- Liveness behavior is **explicitly policy-driven**
- Forced reconnects are intentional, ordered, and observable
- No background timers, implicit pings, or hidden recovery logic exist

If a connection remains healthy, it is because **real protocol traffic was
observed** — not because the Core assumed health.

---

## Liveness Policies

### Passive

- The Session observes liveness only
- No protocol heartbeats are emitted
- If observable traffic stops, the Connection will force a reconnect

This mode is useful for:

- Diagnostics and inspection
- Observational or read-only clients
- Environments where the server emits frequent data naturally

Reconnects in this mode are **expected behavior**, not failures.

---

### Active

- The Session reacts to `LivenessThreatened` signals
- Explicit protocol-level `ping` messages are emitted
- Server responses restore observable traffic
- Forced reconnects are *attempted to be avoided*, not suppressed

This mode is suitable for:

- Sparse or subscription-based feeds
- Long-lived idle connections
- Explicit protocol ownership of connection health

Liveness is preserved **only if traffic is actually observed**.

---

## Scope and Constraints

This example intentionally does **not**:

- Modify transport timeout values
- Inject artificial or fake heartbeats
- Suppress disconnects
- Prevent reconnects

Wirekrak never invents traffic.  
Liveness is satisfied **only** through real protocol messages.

---

## How to Run the Example

1. Run the example with a valid Kraken WebSocket URL.
2. Observe **Phase A (Passive policy)**:
   - No protocol heartbeats are sent.
   - Forced reconnects may occur due to silence.
3. Observe **Phase B (Active policy)**:
   - The Session emits protocol pings.
   - Observable traffic stabilizes liveness.

Both behaviors are correct, intentional, and enforced.

---

## Summary

> **Connection enforces liveness invariants.  
> Protocols decide how (or whether) to satisfy them.  
> Silence is unhealthy. Nothing is hidden.**

This example exists to make that contract **explicit, testable, and observable**.
