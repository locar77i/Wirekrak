# Subscription ACK Enforcement (Kraken)

This example demonstrates **strict, ACK-driven subscription enforcement** in
Wirekrak Core when interacting with the Kraken WebSocket API.

Subscription state is never inferred, assumed, or optimized optimistically.
All state transitions are driven exclusively by **protocol acknowledgements**.

---

## Contract Demonstrated

- Subscriptions are **not active** until an explicit ACK is received
- Duplicate subscribe requests are **not merged or deduplicated optimistically**
- Unsubscribe requests issued before ACK resolution are handled deterministically
- Core never fabricates or infers subscription state

If a subscription becomes active, it is because **the protocol explicitly
acknowledged it**.

---

## What the Example Does

1. Connects to Kraken.
2. Issues **two identical subscribe requests** for the same symbol.
3. Immediately issues an **unsubscribe request**.
4. Observes:
   - Subscription rejections from the protocol
   - Deterministic pending/active state transitions
   - No optimistic activation at any point

The example continuously prints the internal subscription manager state:

```
[STATE] active=<N> pending=<M>
```

This makes state evolution explicit and observable.

---

## Scope and Constraints

This example intentionally does **not**:

- Retry or merge duplicate subscriptions
- Assume subscription success before ACK
- Hide protocol rejections
- Correct user behavior implicitly

Wirekrak Core enforces correctness by **reflecting protocol reality exactly**.

---

## How to Run the Example

1. Run the example with a valid Kraken symbol:
   ```
   --symbol LTC/EUR
   ```
2. Observe:
   - Duplicate subscribe rejection
   - Deterministic unsubscribe resolution
   - Zero active subscriptions unless ACKed

---

## Expected Observations

- One subscribe request is rejected by Kraken
- The unsubscribe resolves cleanly even if ACK ordering differs
- `active` remains zero unless explicitly ACKed
- `pending` drains deterministically

---

## Summary

> **Subscription state in Wirekrak Core is strictly ACK-driven.  
> No optimistic assumptions are made.  
> Protocol truth is enforced verbatim.**

This example exists to make that contract observable, testable, and undeniable.
