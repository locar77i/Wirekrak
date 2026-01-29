# Minimal Poll-Driven Execution

This example demonstrates the most fundamental Wirekrak Core contract:

> **Nothing happens unless `poll()` is called.**

---

## Contracts Demonstrated

- Core execution is explicit and synchronous
- Subscriptions are protocol requests
- Message delivery is driven exclusively by `poll()`
- Lifecycle and termination are user-controlled

---

## Summary

> **Wirekrak Core exposes its execution model directly.  
> There are no hidden threads, timers, or background loops.**
