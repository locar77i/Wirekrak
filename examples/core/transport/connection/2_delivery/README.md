# Example 2 — Connection vs Transport Semantics

This example demonstrates a **fundamental design boundary in Wirekrak**:

> Receiving data on the wire is not the same as delivering data to the application.

Wirekrak makes this distinction explicit, observable, and measurable.

---

## What this example proves

- WebSocket **transport** may receive messages continuously.
- The **Connection layer** only forwards messages when explicitly instructed.
- Message delivery is a **policy decision**, not an automatic side effect.
- Telemetry correctly reflects this separation.

In short:

> **Observation ≠ Delivery**

---

## What this example is NOT

- ❌ It is not a subscription tutorial  
- ❌ It does not guarantee message delivery  
- ❌ It does not auto-install callbacks  
- ❌ It does not infer user intent  

If you expect messages to be delivered just because they arrive on the wire,
this example exists to correct that assumption.

---

## Phases explained

### Phase A — Transport receives, Connection forwards nothing

- A WebSocket connection is opened.
- A valid subscription is sent.
- **No message callback is installed**.
- The transport receives messages.
- The Connection intentionally forwards **zero** messages.

Expected outcome:
- `RX messages > 0`
- `Messages forwarded == 0`

This is correct behavior.

---

### Phase B — Explicit delivery enabled

- A message callback is installed.
- The same incoming messages are now:
  - Observed by the transport
  - Forwarded by the Connection
  - Delivered to user code

Expected outcome:
- `Messages forwarded` starts increasing
- Forwarded messages are visible in logs

Delivery is now intentional.

---

## What to watch for in the telemetry

| Metric | Meaning |
|------|--------|
| RX messages | Frames received from the network |
| Messages forwarded | Messages delivered to user code |
| RX ≥ Forwarded | Always true, by design |

If these numbers were equal by default,
the Connection layer would be leaking policy.

---

## Design contract demonstrated

- Transport reports **facts**
- Connection enforces **policy**
- Delivery requires **explicit intent**
- Telemetry reflects reality, not assumptions

Nothing is hidden.
Nothing is inferred.

---

## Why this matters

This separation enables:

- Passive observation
- Late-binding consumers
- Deterministic replay
- Precise telemetry
- Testable behavior

And prevents:

- Accidental message handling
- Implicit side effects
- Double counting
- Hidden coupling

---

## TL;DR

If no one is listening,  
**messages are not delivered — by design.**

Wirekrak separates transport from intent.

---

Wirekrak enforces correctness —  
it does not hide responsibility.
