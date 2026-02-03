# Kraken Session Liveness Enforcement

This example demonstrates **protocol-level liveness enforcement** in Wirekrak
using the `protocol::kraken::Session`.

It shows how **Wirekrak Core enforces connection health**, while **delegating
responsibility for maintaining liveness to the protocol** via an explicit policy.

The Session does not guess, infer, or hide behavior — it reacts deterministically
to observable signals.

---

## Contract Demonstrated

- The transport layer enforces liveness invariants
- The protocol layer is responsible for satisfying them
- Liveness behavior is **policy-driven**, not implicit
- Forced reconnects are intentional and observable
- No background timers or hidden pings exist

If a connection remains healthy, it is because **the protocol produced liveness
signals** — not because the Core assumed it.

---

## Liveness Policies

### Passive

- The Session observes liveness only
- No protocol heartbeats are emitted
- If traffic stops, the Connection will force a reconnect

This mode is useful for:
- Diagnostics
- Observational clients
- Environments where the server emits frequent data naturally

---

### Active

- The Session reacts to liveness warnings
- Protocol-level `ping` messages are emitted
- Server responses restore observable traffic
- Forced reconnects are avoided

This mode is suitable for:
- Sparse or subscription-based feeds
- Long-lived idle connections
- Explicit protocol ownership of health

---

## Scope and Constraints

This example intentionally does **not**:

- Modify transport timeouts
- Inject artificial heartbeats
- Fake liveness signals
- Prevent reconnects

Wirekrak Core does not invent traffic.  
Liveness is satisfied **only** through real protocol messages.

---

## How to Run the Example

1. Run the example with a valid Kraken WebSocket URL.
2. Observe Phase A (Passive policy):
   - The connection may reconnect due to missing traffic.
3. Observe Phase B (Active policy):
   - The Session emits protocol pings.
   - Liveness stabilizes without reconnects.

Both behaviors are correct and intentional.

---

## Summary

> **Wirekrak enforces liveness.  
> Protocols decide how to satisfy it.  
> Nothing is automatic. Nothing is hidden.**

This example exists to make that contract explicit, testable, and observable.
