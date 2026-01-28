# Example 3 — Error & Close Lifecycle

This example demonstrates Wirekrak’s **error and close lifecycle guarantees** and
how failures propagate *deterministically* through the **transport layer** and
the **Connection layer**.

> **Core truth:**  
> Errors may vary.  
> Closures must not.

---

## What this example proves

- Errors and close events are **distinct signals**
- Errors may occur **before** a close
- A logical disconnect is reported **exactly once**
- Close events are **never double-counted**
- Retry decisions are made **after lifecycle resolution**

This example exists to validate **lifecycle correctness**, not recovery speed.

---

## What this example is NOT

- ❌ It is not testing reconnect backoff tuning
- ❌ It is not testing protocol semantics
- ❌ It is not hiding transport failures
- ❌ It is not smoothing lifecycle events

If you expect “best effort” or “maybe-close” semantics,
this example is designed to correct that assumption.

---

## Scenario overview

1. Open a WebSocket connection  
2. Optionally send a subscription  
3. Allow traffic and/or errors to occur  
4. Observe message, error, and disconnect callbacks  
5. Force a local `close()`  
6. Dump telemetry  

Nothing is mocked.  
Nothing is inferred.  
Only facts are reported.

---

## Callback ordering (this matters)

Expected rules:

- Transport errors may occur **zero or more times**
- Errors are observed **before** closure
- `on_disconnect` fires **exactly once**
- Retry scheduling occurs **after** lifecycle resolution

---

## Telemetry interpretation

- **Disconnect events** must always equal **1**
- **Receive errors** reflect transport failures
- **Close events** are deduplicated

---

## TL;DR

Errors can happen many times.  
Close happens **once**.

Wirekrak reports reality.  
It does not hide lifecycle truth.
