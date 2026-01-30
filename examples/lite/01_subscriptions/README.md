# Lite Examples — Subscriptions

These examples demonstrate how to **configure and control subscriptions**
using the Wirekrak Lite client.

They build directly on the quickstart examples and introduce runtime control
over which symbols are subscribed and how the application shuts down.

The Lite mental model remains unchanged:
- One client
- One polling loop
- Callback-driven data consumption

---

## What These Examples Demonstrate

- Subscribing to multiple symbols
- Runtime configuration via command-line arguments
- Explicit error handling using `on_error()`
- Clean unsubscribe and shutdown semantics

---

## Available Examples

### `trades.cpp`
Configurable trade subscriptions.

- Subscribe to trade updates for one or more symbols
- Control snapshot behavior at startup
- Observe ordered trade callbacks

---

### `book.cpp`
Configurable order book subscriptions.

- Subscribe to book updates for one or more symbols
- Observe snapshot vs incremental updates
- Cleanly unsubscribe on shutdown

---

These examples show how Lite is typically used in real programs, without
introducing additional concurrency, transport, or protocol concepts.
