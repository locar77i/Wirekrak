# Minimal Stateful Stream (Order Book)

This example demonstrates that **stateful order book streams** do not change
Wirekrak Core’s execution model.

---

## Contracts Demonstrated

- Order book subscriptions are explicit protocol requests
- Statefulness does not imply background execution
- Message delivery is driven exclusively by `poll()`
- Lifecycle and termination are user-controlled

---

## Summary

> **Wirekrak Core treats stateful and stateless streams uniformly at the
> execution level. There is no hidden machinery.**
