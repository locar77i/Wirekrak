# Protocol-Level Book Subscription

This example demonstrates how Wirekrak Core handles **stateful order book
subscriptions** with explicit parameters and ACK tracking.

---

## Contracts Demonstrated

- Book subscriptions are parameterized protocol contracts
- Depth and snapshot are enforced explicitly
- Data-plane callbacks are routed deterministically
- Control-plane events are independent of subscriptions
- poll() drives all execution

---

## Summary

> **Wirekrak Core enforces order book correctness by honoring explicit protocol
> contracts, not inferred state.**
