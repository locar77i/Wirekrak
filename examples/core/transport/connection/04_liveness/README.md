# Example 4 — Heartbeat-driven Liveness (Protocol-managed)

This example demonstrates Wirekrak’s **liveness model** and the strict
separation of responsibilities between the **Connection layer** and
**protocol logic**.

It also demonstrates **cooperative liveness**, where the Connection
*signals* impending failure and the Protocol decides how to react.

> **Core truth:**  
> Wirekrak enforces liveness.  
> Protocols must *earn* it.

---

## What this example proves

- A WebSocket connection is **not** considered healthy unless
  *observable protocol traffic* is present.
- Passive connections may be closed **intentionally and deterministically**.
- Reconnects are **not errors** — they are enforcement.
- Protocols may satisfy liveness **reactively**, not just periodically.
- Early liveness warnings enable *just-in-time* protocol intervention.

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

### Phase C — Protocol-managed liveness (reactive)

- A **liveness warning hook** is installed.
- When the Connection detects that liveness is *about to expire*:
  - It emits a warning with the remaining time budget.
- The protocol reacts by sending a **ping** only when warned.
- The server responds.
- Observable traffic resumes.
- Forced reconnects are avoided.

The protocol now satisfies the liveness contract **just-in-time**.

---

## What to watch for in the logs

- `Liveness warning`  
  → Connection signaling impending enforcement

- `Sending protocol ping`  
  → Protocol reacting to the warning

- `Forcing reconnect`  
  → Missing or ignored liveness signals (expected)

- `Retry context`  
  → Deterministic recovery, not failure

- Stable connection after warnings  
  → Correct cooperative behavior achieved

---

## How to interpret the outcome

| Observation | Meaning |
|------------|--------|
| Repeated reconnects | Protocol emits no keepalive |
| Warnings followed by pings | Protocol reacting correctly |
| Reconnects stop after warnings | Liveness satisfied |
| No warnings or reconnects | Exchange emits implicit heartbeats |
| Reconnects despite warnings | Ping format or timing is incorrect |

---

## Design contract demonstrated

- Connection enforces health invariants
- Connection *signals*, but never acts on behalf of the protocol
- Protocols decide **if**, **when**, and **how** to emit traffic
- Forced reconnects are intentional, observable, and recoverable
- Wirekrak never invents keepalive behavior

This contract is **intentional and non-negotiable**.

---

## TL;DR

If your protocol stays silent,  
**Wirekrak will disconnect — by design.**

If your protocol listens and reacts,  
**Wirekrak will stay out of the way.**

Wirekrak enforces correctness.  
It does not hide responsibility.

---

⬅️ [Back to Connection Examples](../Examples.md#liveness)
