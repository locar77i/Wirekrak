# Example 4 — Heartbeat & Liveness Responsibility

This example demonstrates Wirekrak’s **deterministic liveness model** and the
strict separation of responsibilities between the **Connection layer** and
**protocol logic**.

It showcases **cooperative liveness**, where the Connection enforces health
invariants and the Protocol decides how to maintain them.

> **Core truth:**  
> Wirekrak enforces liveness.  
> Protocols must *maintain* it.

---

## What this example proves

- A WebSocket connection is **not considered healthy** without
  observable protocol traffic.
- Passive connections may be closed **intentionally and deterministically**.
- Reconnects are enforcement, not failures.
- Liveness warnings provide early visibility before expiration.
- Protocols may satisfy liveness **reactively**, not just periodically.
- Explicit data-plane consumption does not imply health.

---

## What this example is NOT

This example intentionally does **not**:

- ❌ Subscribe to market data
- ❌ Assume exchange-specific heartbeat behavior
- ❌ Auto-ping or hide reconnects
- ❌ Infer liveness from TCP state
- ❌ Suppress inactivity

If you expect a passive WebSocket to remain healthy indefinitely,
this example is designed to challenge that assumption.

---

## Phase breakdown

### Phase A — Passive silence

- A WebSocket connection is opened.
- No protocol pings or subscriptions are sent.
- The exchange may emit initial messages and then go silent.
- If no traffic is observed within the configured window:
  - `LivenessThreatened` is emitted.
  - Continued silence triggers forced disconnect.
  - Retry is scheduled deterministically.

This behavior is intentional.

---

### Phase B — Protocol-managed heartbeat (reactive)

- The protocol listens for `LivenessThreatened`.
- Upon warning, it sends an explicit ping payload.
- The server responds.
- Observable traffic resumes.
- Liveness expiration is avoided.
- Reconnect cycles stop.

The protocol now satisfies the liveness contract just-in-time.

Nothing is inferred.  
Nothing is automatic.  
Only observable traffic resets liveness.

---

## What to watch for in the logs

- `Liveness warning`  
  → Connection signaling impending enforcement

- `Sending protocol ping`  
  → Protocol reacting to warning

- `Disconnected` followed by retry  
  → Silence exceeded the liveness window

- Stable connection after warnings  
  → Correct cooperative behavior

---

## Interpreting the outcome

| Observation | Meaning |
|------------|--------|
| Repeated reconnects | Protocol emits no keepalive |
| Warnings followed by pings | Protocol reacting correctly |
| Reconnects stop after warnings | Liveness satisfied |
| No warnings or reconnects | Exchange emits implicit traffic |
| Reconnects despite pings | Ping format/timing incorrect |

---

## Design contract demonstrated

- Connection enforces health invariants.
- Connection emits warnings before expiration.
- Connection never invents keepalive traffic.
- Protocols decide **if**, **when**, and **how** to send heartbeats.
- Forced reconnects are observable and deterministic.

This contract is intentional and explicit.

---

## Liveness invariant

No observable traffic  
→ LivenessThreatened  
→ Expiration  
→ Forced reconnect

Observable traffic before expiration  
→ Stability maintained

---

## TL;DR

If your protocol stays silent,  
**Wirekrak will disconnect — by design.**

If your protocol listens and reacts,  
**Wirekrak remains stable.**

Wirekrak enforces correctness.  
It does not hide responsibility.

---


⬅️ [Back to Connection Examples](../Examples.md#liveness)
