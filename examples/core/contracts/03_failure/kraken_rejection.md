# Kraken Subscription Rejection Is Final

This example demonstrates **authoritative protocol-level rejection handling**
in Wirekrak using the `protocol::kraken::Session`.

It shows that **rejections are surfaced verbatim and treated as final intent
resolution**, independent of transport reconnects or liveness enforcement.

Wirekrak does not retry, repair, reinterpret, or replay rejected intent.

---

## Contract Demonstrated

- Protocol rejections are **authoritative**
- Rejected requests are **never retried**
- Rejected symbols are **not replayed after reconnect**
- No implicit correction or symbol dropping occurs
- Transport reconnection is **orthogonal** to protocol intent
- Transport progress is observable via **epoch advancement**

If a request is rejected, it is because **the protocol said no** — and that
decision is preserved until the user explicitly issues new intent.

---

## What the Example Does

1. Connects a Kraken `Session`
2. Issues a subscription request for an **invalid symbol**
3. Observes the rejection notice emitted by the protocol
4. Inspects subscription manager state before and after rejection
5. Allows liveness-driven disconnects and reconnects to occur naturally
6. Demonstrates that the rejected intent is **not replayed**, even as
   transport epochs advance

The example remains passive and observational — it never interferes with
transport behavior or recovery.

---

## Scope and Constraints

This example intentionally does **not**:

- Retry the rejected request
- Replace, normalize, or correct invalid symbols
- Mask or downgrade protocol errors
- Suppress reconnects or liveness enforcement
- Inject keep-alives or protocol traffic

Wirekrak does not invent intent or guess user meaning.  
Rejections are terminal unless the user submits new intent.

---

## How to Run the Example

1. Run the example with a valid Kraken WebSocket URL.
2. Observe the attempted subscription using an invalid symbol.
3. Observe the rejection notice and subscription manager state.
4. Allow the connection to disconnect and reconnect (e.g. via liveness).
5. Observe that **no subscription replay occurs**, even across reconnects.

Transport reconnects may happen — rejected intent does not.

---

## Observable Signals

You should observe:

- A rejection notice emitted by the protocol
- Pending subscriptions dropping to zero
- Active subscriptions remaining zero
- Transport reconnects occurring independently
- Transport epoch increasing
- No automatic re-subscription attempts

These observations together prove the contract.

---

## Summary

> **Rejections are final.**  
> **Intent is not repaired.**  
> **Reconnects do not change protocol truth.**

This example exists to make that contract explicit, testable, and observable.
