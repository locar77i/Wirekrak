# Wirekrak Lite — Quick Usage Guide

This document provides a **fast, practical overview** of the Wirekrak Lite API.
It is intended for users who want to **consume market data quickly** without understanding protocol internals or infrastructure concepts.

For design goals and guarantees, see the **Lite Audience Contract**.

---

## When to Use Lite

Use Wirekrak Lite if you:
- Want real-time market data streams (trades, order books)
- Prefer minimal setup and safe defaults
- Do not need protocol-level control, recovery, or replay

If you need lifecycle orchestration, recovery semantics, or protocol-level access, use **Wirekrak Core** instead.

---

## Include Header

```cpp
#include <wirekrak.hpp>
```

This header exposes the Lite API as the primary SDK entry point.

---

## Basic Workflow

The Lite API follows a simple, explicit workflow:

1. Create a `wirekrak::lite::Client`
2. Register error handling (recommended)
3. Subscribe to one or more market data streams
4. Call `poll()` regularly to process events
5. Disconnect when done

Lite hides protocol sessions, transport details, and exchange-specific behavior.

---

## Creating a Client

```cpp
wirekrak::lite::Client client;
```

The client uses safe, opinionated defaults.
The underlying exchange implementation is an internal detail.

You may also provide an explicit configuration:

```cpp
wirekrak::lite::client_config cfg;
cfg.endpoint = "wss://ws.kraken.com/v2";

wirekrak::lite::Client client{cfg};
```

---

## Error Handling

Register an error handler before connecting:

```cpp
client.on_error([](const wirekrak::lite::Error& err) {
    // handle error
});
```

Errors are surfaced explicitly.
Lite does not silently retry or hide failures.

---

## Connecting and Polling

```cpp
if (!client.connect()) {
    // connection failed
}

while (running) {
    client.poll();
}
```

Lite does not spawn background threads.
All progress is driven by explicit calls to `poll()`.

---

## Subscribing to Trades

```cpp
client.subscribe_trades(
    { "BTC/USD" },
    [](const wirekrak::lite::Trade& trade) {
        // handle trade
    }
);
```

- Symbols are domain-level identifiers
- Callbacks receive stable domain value types
- No protocol frames or raw messages are exposed

```Note``` Domain types are re-exported directly under wirekrak::lite for convenience.
The domain namespace is an internal grouping and is not required for Lite usage.

---

## Subscribing to Order Book Updates

```cpp
client.subscribe_book(
    { "BTC/USD" },
    [](const wirekrak::lite::BookLevel& level) {
        // handle book update
    }
);
```

Order book updates represent **observations**, not protocol deltas.

```Note``` Domain types are re-exported directly under wirekrak::lite for convenience.
The domain namespace is an internal grouping and is not required for Lite usage.

---

## Unsubscribing

```cpp
client.unsubscribe_trades({ "BTC/USD" });
client.unsubscribe_book({ "BTC/USD" });
```

---

## Shutdown

```cpp
client.disconnect();
```

Disconnecting releases all underlying resources.

---

## Guarantees and Limitations

Lite provides:
- Domain-level market data
- Exchange-neutral public API
- Explicit, predictable control flow

Lite does NOT provide:
- Durability or replay
- Recovery guarantees
- Ordering guarantees across reconnects
- Exchange-specific features

For stronger guarantees, use **Wirekrak Core**.

---

## Summary

Wirekrak Lite is designed for **fast, safe market data consumption**.

If you can describe your needs as:

> “I want market data, not protocol behavior”

then Lite is the correct API.

---

## Related Documents

➡️ **[Audience Contract](./AUDIENCE_CONTRACT.md)**

➡️ **[Public API Stability](./PUBLIC_API_STABILITY.md)**

---

⬅️ [Back to README](./README.md#quick-usage)
