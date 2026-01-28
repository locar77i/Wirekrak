# Example 0 — Minimal Connection Lifecycle

This example is the **starting point for Wirekrak**.

It demonstrates the *smallest correct program* that opens, drives, and closes
a Wirekrak connection — **without any protocol logic**.

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
- Observes lifecycle callbacks
- Closes the connection cleanly
- Dumps connection and transport telemetry

---

## What this example does NOT do

- ❌ No subscriptions
- ❌ No protocol messages
- ❌ No heartbeats
- ❌ No liveness tuning
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

If you do not call `poll()`, **nothing happens**.

Example 0 makes this unmistakably clear.

---

## Key concepts introduced

### 1. Connection owns lifecycle

The `Connection`:
- Tracks logical state
- Emits lifecycle callbacks
- Enforces correctness
- Owns telemetry

The transport only reports what happens on the wire.

---

### 2. `poll()` drives everything

Wirekrak is **poll-driven**.

Calling `poll()`:
- Advances the transport
- Processes incoming frames
- Executes state transitions
- Delivers callbacks

If you forget to poll, the system stalls by design.

---

### 3. Telemetry is mandatory

Telemetry is not optional in Wirekrak.

This example shows:
- Connection lifecycle counters
- WebSocket traffic statistics
- Error and close tracking

Nothing is inferred.
Nothing is hidden.

---

## Expected output behavior

When running Example 0, you should observe:

- Exactly one connect event
- Exactly one disconnect event
- No forwarded messages
- Clean shutdown
- Deterministic telemetry

Any deviation indicates:
- A misuse of the API, or
- A real transport issue

---

## How this fits in the example series

Example 0 is the foundation.

| Example | Focus |
|-------|------|
| Example 0 | Minimal lifecycle & polling |
| Example 1 | Message shape & fragmentation |
| Example 2 | Transport vs delivery |
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

Everything else is layered on top.

---

Wirekrak enforces correctness.  
It does not hide responsibility.
