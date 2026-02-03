# Kraken Subscription Rejection Is Final

This example demonstrates **authoritative protocol-level rejection handling**
in Wirekrak using the `protocol::kraken::Session`.

It shows that **rejections are surfaced verbatim and treated as final intent
resolution**, even across transport reconnects.

Wirekrak does not retry, repair, reinterpret, or replay rejected intent.

---

## Contract Demonstrated

- Protocol rejections are authoritative
- Rejected requests are **never retried**
- Rejected symbols are **not replayed after reconnect**
- No implicit correction or symbol dropping occurs
- Transport reconnection is independent from protocol intent

If a request is rejected, it is because **the protocol said no** — and that
decision is preserved.

---

## What the Example Does

1. Connects a Kraken Session
2. Issues a subscription request for an **invalid symbol**
3. Observes the rejection notice
4. Shows subscription manager state before and after rejection
5. Allows liveness-driven disconnects and reconnects
6. Demonstrates that the rejected intent is **not replayed**

The example remains passive and observational — it does not interfere with
transport behavior.

---

## Scope and Constraints

This example intentionally does **not**:

- Retry the rejected request
- Replace or correct invalid symbols
- Suppress reconnects
- Mask protocol errors

Wirekrak does not invent intent or guess user meaning.  
Rejections are terminal unless the user issues a new request.

---

## How to Run the Example

1. Run the example with a valid Kraken WebSocket URL.
2. Observe the subscription attempt with an invalid symbol.
3. Observe the rejection notice and subscription manager state.
4. Wait for a reconnect (e.g. liveness-triggered).
5. Observe that **no subscription replay occurs**.

The rejected intent remains rejected.

---

## Observable Signals

You should see:

- A rejection notice from the protocol
- Pending subscriptions dropping to zero
- Active subscriptions remaining zero
- Reconnects occurring without replay
- No additional subscribe messages sent automatically

---

## Summary

> **Rejections are final.  
> Intent is not repaired.  
> Reconnects do not change protocol truth.**

This example exists to make that contract explicit, testable, and observable.
