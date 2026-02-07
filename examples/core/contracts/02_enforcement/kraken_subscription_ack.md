# Subscription ACK Enforcement (Kraken)

This example demonstrates **strict, ACK-driven subscription enforcement** in
Wirekrak Core when interacting with the Kraken WebSocket API.

Subscription state is never inferred, assumed, or optimized optimistically.
All subscription state transitions are driven exclusively by **protocol
acknowledgements (ACKs)** and explicit protocol rejections.

Transport progress (connections, reconnects, epochs) is **orthogonal** to
subscription correctness.

---

## Contract Demonstrated

- Subscriptions are **not active** until explicitly ACKed by the protocol
- Duplicate subscribe intents are **surfaced, not merged**
- Unsubscribe requests issued before ACK resolution are handled deterministically
- Rejected subscriptions are **final and never replayed**
- Core never fabricates, repairs, or infers subscription state

If a subscription becomes active, it is because **the protocol said yes**.

---

## What the Example Does

1. Connects a Kraken session.
2. Issues **two identical subscribe requests** for the same symbol set.
3. Immediately issues an **unsubscribe request**.
4. Observes:
   - Protocol rejection notices
   - Deterministic pending → resolved transitions
   - Zero optimistic activation at all times

The example continuously prints the internal subscription manager state:

```
[example] Trade subscriptions: active=<N> - pending=<M>
```

This makes subscription state progression explicit and observable.

---

## Scope and Constraints

This example intentionally does **not**:

- Retry rejected subscriptions
- Merge or deduplicate user intent
- Assume subscription success before ACK
- Mask or reinterpret protocol rejections
- Tie subscription correctness to transport connectivity

Wirekrak Core enforces correctness by **reflecting protocol reality verbatim**.

---

## How to Run the Example

1. Run the example with a valid Kraken WebSocket URL.
2. Provide one or more symbols, for example:
   ```
   --symbol LTC/EUR
   ```
3. Observe:
   - Duplicate subscribe rejection
   - Deterministic unsubscribe resolution
   - No active subscriptions unless explicitly ACKed

---

## Expected Observations

- One subscribe intent is rejected by the protocol
- The unsubscribe resolves cleanly regardless of ACK ordering
- `active` remains zero unless the protocol acknowledges success
- `pending` drains deterministically to zero

---

## Summary

> **Subscription state in Wirekrak Core is strictly ACK-driven.**  
> **No optimistic assumptions are made.**  
> **Transport progress does not change protocol truth.**

This example exists to make subscription correctness **observable, testable,
and undeniable**.
