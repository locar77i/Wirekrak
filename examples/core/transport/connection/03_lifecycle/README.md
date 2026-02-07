# Example 3 — Failure, Disconnect & Close Ordering

This example demonstrates Wirekrak’s **failure, disconnect, and close ordering guarantees**
and how faults propagate *deterministically* through the **transport layer** and the
**Connection layer**.

> **Core truth:**  
> Errors may vary.  
> **Disconnect is singular.**  
> **Closure is exact.**

---

## What this example proves

This example validates **observable lifecycle correctness**, not recovery heuristics.

Specifically, it proves that:

- Transport errors and disconnects are **distinct observable facts**
- Errors may occur **before** a disconnect
- A logical disconnect is emitted **exactly once**
- Physical transport close events are **idempotent**
- Retry decisions occur **after disconnect resolution**
- Lifecycle signals are **ordered, explicit, and non-ambiguous**

---

## What this example is NOT

This example intentionally does **not**:

- ❌ Tune retry backoff or reconnection latency
- ❌ Test protocol or subscription semantics
- ❌ Suppress or smooth transport failures
- ❌ Guess intent or infer health state

If you expect “best-effort”, “eventual”, or “maybe-closed” semantics,
this example exists to correct that assumption.

---

## Scenario overview

1. Open a WebSocket connection  
2. Optionally send a raw payload (protocol-agnostic)  
3. Allow traffic, errors, or remote close to occur  
4. Observe **connection::Signal** ordering  
5. Force a local `close()`  
6. Dump transport and connection telemetry  

Nothing is mocked.  
Nothing is inferred.  
Only **observable facts** are reported.

---

## Signal ordering (this matters)

Expected rules:

- Transport errors may occur **zero or more times**
- Errors are observed **before logical disconnect resolution**
- `connection::Signal::Disconnected` is emitted **exactly once**
- Physical close events may occur before or after error reporting
- Retry scheduling (if any) occurs **after disconnect**

No lifecycle signal is emitted twice.
No ordering is guessed.

---

## Telemetry interpretation

When reading the output:

- **Disconnect events**
  - Must always equal **1** per lifecycle
- **Receive errors**
  - Explain *why* the connection failed
- **Close events**
  - Reflect physical socket teardown
  - Are deduplicated and idempotent

Telemetry reflects **reality**, not assumptions.

---

## Key invariant

Errors may happen many times.  
**Disconnect happens once.**  
Close is exact.

If these ever disagree,
the system is lying to you.

---

## TL;DR

Wirekrak does not hide failure.  
It does not merge events.  
It does not guess.

It models failure **precisely**, so you can trust what you observe.

---

⬅️ [Back to Connection Examples](../Examples.md#lifecycle)
