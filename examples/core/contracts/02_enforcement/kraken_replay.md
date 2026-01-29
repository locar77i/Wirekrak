# Subscription Replay on Reconnect

This example demonstrates **subscription replay enforced by Wirekrak Core**.

Active subscriptions are recorded internally by the Core session.  
When a **real transport disconnect** occurs and the connection later reconnects,
these subscriptions are **replayed automatically** without user intervention.

The integrator does **not** resubscribe, re-register callbacks, or manage replay
logic explicitly.

---

## Contract Demonstrated

- Subscriptions are captured at the Core level
- Replay is automatic and deterministic
- No user code is required to restore subscriptions
- Callback delivery resumes after reconnect

If callbacks resume after a reconnect, it is because **Core enforced replay**.

---

## Scope and Constraints

This example intentionally does **not**:

- Force or simulate a disconnect
- Fabricate liveness timeouts
- Trigger reconnects programmatically

Wirekrak Core does not invent failures.  
Replay is observed **only** when a real transport reconnect occurs.

---

## How to Run the Example

1. Run the program and wait for data.
2. Disable network connectivity (e.g. airplane mode).
3. Re-enable network connectivity.

When the transport reconnects, previously active subscriptions are replayed
automatically and callbacks resume **without any changes to user code**.

---

## Summary

> **On reconnect, Wirekrak Core automatically replays active subscriptions.  
> Replay is passive, deterministic, and requires no user intervention.**

This example exists to make that contract observable and executable.
