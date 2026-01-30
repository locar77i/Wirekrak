# Protocol-Level Trade Subscription

This example demonstrates how Wirekrak Core handles **explicit protocol
subscriptions** with ACK tracking and deterministic message routing.

---

## Contracts Demonstrated

- Subscriptions are explicit protocol requests
- ACKs are tracked internally by Core
- Data-plane callbacks are routed deterministically
- Control-plane events are independent of subscriptions
- poll() drives all execution

---

## Summary

> **Wirekrak Core exposes protocol semantics directly.  
> Nothing is inferred, hidden, or automated outside the contract.**
