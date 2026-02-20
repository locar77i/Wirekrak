# Example 3 — Failure, Disconnect & Close Ordering

This example demonstrates Wirekrak’s **deterministic failure model**
and the strict ordering guarantees between:

- Transport errors  
- Logical disconnect  
- Physical close  
- Retry scheduling  

> **Core truth:**  
> Errors may vary.  
> **Disconnect is singular.**  
> **Closure is exact.**

Failure is not exceptional in Wirekrak.  
It is observable, ordered, and verifiable.

---

## What this example proves

This example validates **lifecycle correctness under failure**, not recovery heuristics.

Specifically, it proves that:

- Transport errors and disconnects are **distinct observable facts**
- Errors may occur **before** logical disconnect
- Logical disconnect is emitted **exactly once**
- Physical transport close is **idempotent**
- Retry decisions occur **after disconnect resolution**
- Lifecycle signals are **ordered, explicit, and non-ambiguous**
- Data-plane consumption does not interfere with lifecycle correctness

---

## What this example is NOT

This example intentionally does **not**:

- ❌ Tune retry backoff or reconnection latency
- ❌ Test protocol or subscription correctness
- ❌ Smooth or suppress transport failures
- ❌ Infer health state
- ❌ Hide transport-level instability

If you expect “best-effort”, “eventual”, or “maybe-closed” semantics,
this example exists to correct that assumption.

---

## Scenario overview

1. Open a WebSocket connection  
2. Optionally send a raw payload (protocol-agnostic)  
3. Allow traffic, errors, or remote close to occur  
4. Explicitly pull data-plane messages (`peek_message()` / `release_message()`)  
5. Observe `connection::Signal` ordering  
6. Force a local `close()`  
7. Drain until idle  
8. Dump transport and connection telemetry  

Nothing is mocked.  
Nothing is inferred.  
Only **observable facts** are reported.

---

## Lifecycle ordering (this matters)

The ordering rules are strict:

- Transport errors may occur **zero or more times**
- Errors are observed **before logical disconnect resolution**
- `connection::Signal::Disconnected` is emitted **exactly once**
- Physical close may occur before or after error reporting
- Retry scheduling (if retryable) occurs **after disconnect**

No lifecycle signal is emitted twice.  
No ordering is guessed.

If you ever observe:

- Multiple disconnect signals  
- Retry before disconnect  
- Close counted twice  

The invariant is broken.

---

## Data-plane behavior during failure

Even while errors occur:

- The transport may still deliver messages
- The application must explicitly call:
  
  ```
  peek_message()
  release_message()
  ```

Failure does not “auto-flush” or implicitly consume messages.

Lifecycle correctness and data-plane correctness are independent.

---

## Telemetry interpretation

When reading telemetry:

### Connection telemetry

- **Disconnect events**
  - Must always equal **1 per lifecycle**
- **Retry cycles**
  - Must follow disconnect

### WebSocket telemetry

- **Receive errors**
  - Explain *why* the failure occurred
- **Close events**
  - Reflect physical socket teardown
  - Are deduplicated and idempotent

Telemetry reflects **reality**, not assumptions.

---

## Core invariant

Errors may happen many times.  
**Disconnect happens once.**  
Close is exact.  
Retry follows cause.

If these ever disagree,
the system is lying to you.

---

## TL;DR

Wirekrak does not hide failure.  
It does not merge lifecycle events.  
It does not guess.

It models failure **precisely**, so you can trust what you observe.

---

⬅️ [Back to Connection Examples](../Examples.md#lifecycle)
