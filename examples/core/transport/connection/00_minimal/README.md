# Example 0 — Minimal Connection Lifecycle

This example is the **starting point for Wirekrak**.

It demonstrates the *smallest correct poll-driven program* that opens,
drives, and closes a Wirekrak connection — **without any protocol logic
and without consuming any data-plane messages**.

If this example feels boring, that is intentional.

---

## Purpose

Example 0 exists to answer one question clearly:

> **What is the minimum I must do to use Wirekrak correctly?**

Nothing more.  
Nothing hidden.  
Nothing inferred.

---

## What this example does

- Creates a `Connection`
- Opens a WebSocket URL
- Drives the connection via `poll()`
- Drains lifecycle signals explicitly
- Does **not** pull from the data-plane
- Closes the connection cleanly
- Dumps connection and transport telemetry

---

## What this example does NOT do

- ❌ No subscriptions
- ❌ No protocol messages
- ❌ No data-plane consumption (`peek_message()` is never called)
- ❌ No heartbeat tuning
- ❌ No retry logic demonstration
- ❌ No exchange assumptions

Those concepts are introduced **incrementally** in later examples.

---

## Why this example matters

Wirekrak is **explicit by design**.

There is:

- No background thread
- No hidden event loop
- No automatic progress
- No implicit message delivery

If you do not call `poll()`, **nothing happens**.

Example 0 makes this unmistakably clear.

---

## Key concepts introduced

### 1. Connection owns lifecycle

The `Connection`:

- Tracks logical state
- Emits lifecycle signals
- Enforces correctness
- Owns telemetry

The transport reports only observable wire events.

---

### 2. `poll()` drives everything

Wirekrak is **poll-driven**.

Calling `poll()`:

- Advances the transport
- Processes incoming frames
- Executes state transitions
- Emits lifecycle signals

If you forget to poll, the system stalls — by design.

---

### 3. Signals are edge-triggered

Lifecycle transitions are surfaced via `poll_signal()`.

Signals represent **observable events**, not steady-state conditions.

You must drain them explicitly.

---

### 4. Telemetry reflects facts

Telemetry is not optional in Wirekrak.

This example shows:

- Connection lifecycle counters
- WebSocket traffic statistics
- Error and close tracking

Telemetry reports **facts**, not interpretations.

---

## Expected output behavior

In a stable network environment, you should observe:

- One successful connect
- One disconnect event
- No messages forwarded (no `peek_message()` calls)
- Clean shutdown
- Deterministic telemetry

If retries occur, additional connect attempts may be visible — this is correct behavior.

---

## How this fits in the example series

Example 0 is the foundation.

| Example | Focus |
|----------|-------|
| Example 0 | Minimal lifecycle & polling |
| Example 1 | Message shape & fragmentation |
| Example 2 | Observation vs consumption |
| Example 3 | Error & close invariants |
| Example 4 | Heartbeat-driven liveness |

Each example builds on the previous one.  
None of them invalidate Example 0.

---

## Takeaway

If you understand Example 0, you understand:

- How Wirekrak runs
- Where responsibility lives
- How control flows
- Why behavior is deterministic
- Why consumption is explicit

Everything else is layered on top.

---

Wirekrak enforces correctness.  
It does not hide responsibility.

---

⬅️ [Back to Connection Examples](../Examples.md#minimal)
