# Subscription Replay on Reconnect

This example demonstrates **subscription replay enforced by Wirekrak Core** under
the current, strictly defined replay and progress model.

Subscriptions that have been **explicitly acknowledged by the protocol** are
recorded internally by the Core session. When a **real transport disconnect**
occurs and the connection later reconnects, those acknowledged subscriptions are
**replayed deterministically** by Core.

The integrator does **not** resubscribe, re-register callbacks, or manage replay
logic explicitly.

---

## Contract Demonstrated

- Only **ACKed subscriptions** are eligible for replay
- Replay is triggered by **successful transport reconnection**
- Replay ordering is deterministic and protocol-safe
- User callbacks resume automatically after replay
- No user code participates in reconnect or replay logic

If callbacks resume after a reconnect, it is because **Core enforced replay based
on protocol truth**.

---

## Replay Trigger Model

Replay is **not callback-driven** and **not level-based**.

Instead, replay occurs when:

- The transport establishes a new logical connection lifetime
- The transport `epoch` advances
- Core observes a `Connected` transition

At that point, Core replays all previously **ACKed** subscription intents through
the same request path as the original subscription.

Rejected or unacknowledged intents are **never replayed**.

---

## Scope and Constraints

This example intentionally does **not**:

- Force or simulate disconnects
- Fabricate liveness timeouts
- Trigger reconnects programmatically
- Replay rejected or pending subscriptions

Wirekrak Core does not invent failures and does not guess intent.
Replay is observed **only** after a real transport reconnect.

---

## How to Run the Example

1. Run the program and wait until subscription data is flowing.
2. Disable network connectivity (e.g. airplane mode).
3. Observe a disconnect and retry cycle.
4. Re-enable network connectivity.

When the transport reconnects:

- The transport `epoch` advances
- Core replays ACKed subscriptions
- Callback delivery resumes automatically
- No user resubscription occurs

---

## Expected Observations

- A single subscription request is issued by user code
- A real disconnect occurs
- A reconnect is observed
- Subscriptions are replayed exactly once per reconnect
- Trade callbacks resume without user intervention

---

## Summary

> **On reconnect, Wirekrak Core automatically replays acknowledged subscriptions.  
> Replay is deterministic, protocol-driven, and requires no user intervention.**

This example exists to make the replay contract observable, testable, and
unambiguous.
