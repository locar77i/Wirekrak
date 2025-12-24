# Dispatcher Module Documentation

## Overview

The **Dispatcher** module is a lightweight, type-safe message routing system used to deliver decoded Kraken protocol messages
to the correct user-defined callbacks based on **symbol** and **channel type**.

It acts as the final step in the message pipeline:
> *decoded message → dispatcher → user callbacks*

This design avoids runtime polymorphism and string-based routing, favoring **compile-time channel traits** and **symbol interning**
for performance and correctness.

---

## Goals

- Route messages by **symbol** and **response type**
- Maintain strong compile-time guarantees
- Avoid dynamic casts or string comparisons at dispatch time
- Keep the SDK lightweight and allocation-conscious

---

## Core Concepts

### Symbol Interning

Symbols are converted into a stable `SymbolId` using:

```cpp
SymbolId intern_symbol(Symbol symbol);
```

This ensures:
- Fast hash-map lookup
- Stable identity across reconnects
- No repeated string comparisons during dispatch

---

### Channel Traits

Each response type is mapped to a Kraken channel via `channel_traits.hpp`:

```cpp
channel_of_v<ResponseT> == Channel::Trade
```

This allows the dispatcher to:
- Select the correct handler table at compile time
- Reject unsupported message types early

---

## Class: Dispatcher

### Callback Type

```cpp
template<class ResponseT>
using Callback = std::function<void(const ResponseT&)>;
```

Callbacks are typed per response, ensuring:
- No unsafe casts
- Clear ownership of message semantics

---

### Adding Handlers

```cpp
dispatcher.add_handler<ResponseT>(symbol, callback);
```

**Behavior:**
- Interns the symbol
- Appends the callback to the symbol’s handler list
- Multiple callbacks per symbol are supported

---

### Dispatching Messages

```cpp
dispatcher.dispatch(msg);
```

**Behavior:**
1. Extracts the symbol from the message
2. Interns it to a `SymbolId`
3. Looks up registered handlers
4. Invokes callbacks sequentially

If no handlers exist, dispatch is a no-op.

---

### Removing Handlers (Unsubscribe)

```cpp
dispatcher.remove_symbol_handlers<UnsubscribeAckT>(symbol);
```

Used when:
- Receiving unsubscribe acknowledgements
- Explicitly stopping all listeners for a symbol

This design aligns well with Kraken’s unsubscribe semantics.

---

### Clearing State

```cpp
dispatcher.clear() noexcept;
```

Clears all registered handlers.
Typically used when:
- Reconnecting
- Shutting down the client
- Resetting internal state

---

## Internal Layout

```
Dispatcher
├── trade_handlers_ : map<SymbolId, vector<TradeCallback>>
└── book_handlers_  : map<SymbolId, vector<BookCallback>>
```

Each handler table is selected via compile-time logic.

---

## Threading Model

- Designed for **single-threaded / event-loop** usage
- Not thread-safe by default
- External synchronization required for multi-threaded dispatch

---

## Strengths

- Zero runtime channel branching
- Strong compile-time correctness
- Extremely fast dispatch path

---

## Summary

The Dispatcher provides zero-overhead, type-safe routing of real-time Kraken messages,
enabling SDK users to focus on trading logic instead of plumbing.

It is a compact yet powerful abstraction for message routing in real-time SDKs.
With minor extensions around lifecycle management and observability, it can scale from
hackathon prototype to production-ready infrastructure.

---

⬅️ [Back to README](../../../../README.md#distpatcher)
