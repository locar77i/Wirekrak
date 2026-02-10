# Wirekrak Lite — Layer Architecture

Wirekrak **Lite** is the stable, user-facing façade of the Wirekrak ecosystem.

It provides a **simple, callback-driven API** for consuming real-time market data,
while internally leveraging the deterministic, poll-driven Core engine.

Lite is designed for:
- Application developers
- CLI tools and scripts
- Trading bots and data consumers
- Users who do not want to reason about protocol details

---

## Design Philosophy

Lite follows three strict principles:

### 1. Facade, Not a Framework

Lite **does not introduce new execution models**.

- No background threads
- No hidden schedulers
- No implicit lifecycles
- No protocol inference

All progress is driven explicitly via `poll()` or the provided convenience loops.

### 2. Stable Public API

Lite v1 guarantees:

- Stable domain value layouts (`Trade`, `BookLevel`, …)
- Stable callback signatures
- Exchange-agnostic public API
- No exposure of protocol schemas or Core internals
- No breaking changes without a major version bump

The underlying exchange (Kraken today) is an implementation detail.

### 3. Deterministic Behavior

Lite never guesses user intent.

- Subscriptions are explicit
- Unsubscriptions are explicit
- Shutdown is explicit
- Draining is explicit

If something happens, it is because the user asked for it.

### 4. Symbol-Authoritative Behavior

Lite manages user-visible behavior strictly in terms of **symbols**.

- Callbacks are registered per symbol
- Callbacks are removed per symbol
- Protocol identifiers (e.g. request IDs) never escape Core

Core is responsible for protocol correctness.
Lite is responsible for routing data to user code.

This ensures:
- Clear separation of concerns
- No protocol leakage into the public API
- Deterministic, user-reasonable lifecycle semantics

---

## Execution Model

Lite is **poll-driven**.

```cpp
client.poll();
```

Calling `poll()`:
- Advances the underlying Core session
- Processes protocol control-plane events
- Dispatches data-plane callbacks synchronously

There are **no background threads**.

### Convenience Run Loops

Lite provides optional helpers built on top of `poll()`:

| Method | Termination Authority | Purpose |
|------|----------------------|--------|
| `run_until_idle()` | Library-owned | Drain protocol state and callbacks |
| `run_while(cond)` | User-owned | Steady-state execution |
| `run_until(stop)` | User-owned | Signal / event driven shutdown |

These methods **do not change semantics** — they only remove boilerplate.

---

## Subscriptions

Lite exposes high-level subscription APIs:

```cpp
subscribe_trades(symbols, callback);
subscribe_book(symbols, callback);
```

Each callback:
- Receives exactly one domain object per invocation
- Is invoked synchronously during `poll()`
- Must be non-blocking and fast

Snapshots and incremental updates are clearly tagged.

Unsubscription is explicit:

```cpp
unsubscribe_trades(symbols);
unsubscribe_book(symbols);
```

Lite removes callbacks immediately on unsubscribe intent.

Removal is **symbol-scoped**:
- All callbacks associated with the unsubscribed symbols are removed
- No protocol identifiers are tracked or required at the Lite layer

---

## Error Handling

Protocol-level rejections and failures are surfaced via:

```cpp
client.on_error([](const Error& err) {
    // user-defined handling
});
```

Protocol rejections are surfaced as errors and cause
symbol-scoped callback removal when applicable.

Lite does not retry, suppress, or reinterpret errors.
Errors are facts, not policy.

---

## Quiescence (`is_idle()`)

Lite exposes a compositional quiescence signal:

```cpp
bool idle = client.is_idle();
```

`is_idle()` means:

- Core has no pending protocol work
- Lite has no remaining callbacks to dispatch

It **does not** mean:
- The connection is closed
- The exchange has no subscriptions
- Future data cannot arrive

This signal is intended for graceful shutdown and drain loops.

---

## Threading Model

Lite is **not thread-safe**.

All methods must be called from the same thread:
- `poll()`
- subscription APIs
- run loops
- `is_idle()`

This design favors determinism and ultra-low-latency use cases.

---

## What Lite Is *Not*

Lite intentionally avoids:

- Async/await APIs
- Futures or promises
- Background workers
- Automatic shutdown logic
- Implicit reconnection policies

Those belong either in Core or in user code.

---

## Summary

Wirekrak Lite provides:

- A stable, minimal public API
- Deterministic execution
- Explicit lifecycle control
- Clear separation from protocol logic

Lite is opinionated in one way only:

> **If something happens, it is because you asked for it.**


---

## Related Documents

➡️ [Audience Contract](./AUDIENCE_CONTRACT.md)

➡️ [Public API Stability](./PUBLIC_API_STABILITY.md)

➡️ [Quick Usage Guide](./QUICK_USAGE.md)

---

⬅️ [Back to README](./README.md#architecture)
