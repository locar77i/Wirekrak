# Example 4 — Heartbeat-driven Liveness (Protocol-managed)

This example demonstrates Wirekrak’s **liveness model** and the strict
separation of responsibilities between the **Connection layer** and
**protocol logic**.

> **Core truth:**  
> Wirekrak enforces liveness.  
> Protocols must *earn* it.

---

## What this example proves

- A WebSocket connection is **not** considered healthy unless
  *observable protocol traffic* is present.
- Passive connections may be closed **intentionally and deterministically**.
- Reconnects are **not errors** — they are enforcement.
- Protocol-level pings restore stability.

---

## What this example is NOT

- ❌ It does not subscribe to market data
- ❌ It does not assume exchange-specific behavior
- ❌ It does not auto-ping or hide reconnects
- ❌ It does not infer liveness from TCP state

If you expect a passive WebSocket to stay alive forever,
this example is designed to surprise you.

---

## Phases explained

### Phase A — Passive connection (no protocol traffic)

- A WebSocket connection is opened.
- No subscriptions, no pings.
- Some exchanges emit initial system messages, then go silent.
- Once no traffic is observed within the liveness window:
  - The Connection **forces a disconnect**
  - A reconnect is scheduled

This behavior is intentional.

---

### Phase B — Observable traffic, but no keepalive

- A message callback is installed.
- Incoming messages (if any) are now forwarded and visible.
- However, if traffic stops:
  - Liveness still fails
  - Forced reconnects still occur

Observation alone does **not** imply health.

---

### Phase C — Explicit protocol keepalive

- After a configurable number of reconnects,
  the protocol starts sending periodic **ping messages**.
- Each ping elicits a server response.
- Observable traffic resumes.
- Liveness stabilizes.
- Reconnects stop.

The protocol is now satisfying the liveness contract.

---

## What to watch for in the logs

- `Forcing reconnect`  
  → Missing liveness signals (expected)

- `Retry context`  
  → Deterministic recovery, not failure

- `Sending protocol ping`  
  → Protocol asserting responsibility

- Stable connection after pings  
  → Correct behavior achieved

---

## How to interpret the outcome

| Observation | Meaning |
|------------|--------|
| Repeated reconnects | Protocol emits no keepalive |
| Reconnects stop after pings | Liveness satisfied |
| No reconnects at all | Exchange emits implicit heartbeats |
| Reconnects despite pings | Ping interval or format is wrong |

---

## Design contract demonstrated

- Connection enforces health invariants
- Transport reports facts, not interpretations
- Protocols must emit traffic to stay alive
- Wirekrak will not invent keepalives for you

This behavior is **intentional and non-negotiable**.

---

## TL;DR

If your protocol does not speak,  
**Wirekrak will disconnect — by design.**

Wirekrak enforces correctness.  
It does not hide responsibility.
