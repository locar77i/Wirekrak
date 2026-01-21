
# Wirekrak Market - Layer Architecture

## 1. Purpose and Scope

The **Market layer** is the semantic layer of the Wirekrak SDK.
Its responsibility is to translate *protocol-level market data* into *meaningful,
correctness-aware market abstractions* such as trades, order books, and tickers.

This layer exists because **correct market data is harder than fast market data**.
The Market layer ensures users cannot accidentally build incorrect trading systems
on top of valid but incomplete protocol data.

The Market layer is explicitly designed **prior to implementation** to ensure:
- Clear separation of concerns
- Long-term extensibility
- No architectural leakage into Core or Lite APIs

The Market layer encodes **Kraken market semantics and guarantees** and is not
intended to be exchange-agnostic.

---

## 2. Design Goals

The Market layer is built around the following principles:

1. **Semantic correctness over convenience**
2. **Explicit behavior and policy**
3. **Clear correctness and liveness tradeoffs**
4. **No impact on Core evolution**
5. **No dependency on Lite**
6. **Production-grade correctness guarantees**
7. **Clear audience separation**

---

## 3. Layered Architecture Overview

Wirekrak is structured as a *non-linear layered system*:

```
      Lite API                    Market Layer
(exchange-agnostic facade)   (semantic streams, policies, guarantees)
          ▲                           ▲
          │                           │
          ┴─────────────┌─────────────┴
                        │
                    Core API
                (protocol truth)
                        │
            Transport (WS / HTTP / IO)
```

### Key Observations

- **Core is the single source of protocol truth**
- **Lite and Market are peers**, not parent/child
- **Market depends on Core**
- **Lite depends on Core**
- **Market never depends on Lite**

---

## 4. Audience Segmentation

| Layer  | Intended Audience | Characteristics |
|------|------------------|-----------------|
| Core | ULL / Infra users | Header-only, protocol control, zero hidden behavior |
| Lite | SDK integrators | Simpler APIs, exchange-agnostic, portability |
| Market | Trading & product systems | Semantic guarantees, replay, correctness policies |

---

## 5. Responsibilities by Layer

### Core API
- WebSocket protocol handling
- Message parsing and validation
- Snapshot and delta emission
- Sequence numbers and raw ordering
- Zero semantic interpretation

### Lite API
- Convenience wrappers over Core
- Exchange-agnostic abstractions
- Reduced boilerplate
- No correctness guarantees beyond protocol validity

### Market API
- Market semantics (trades, order books, tickers)
- Replay and resynchronization logic
- Consistency and liveness guarantees
- Behavioral policies
- Stateful stream management

---

## 6. Market Layer Responsibilities

The Market layer is responsible for:

- Translating protocol events into **semantic streams**
- Managing **stateful market objects**
- Detecting and handling **gaps**
- Coordinating **snapshot + delta lifecycles**
- Acting as the **sole authority** for snapshot–delta validation and stream correctness
- Enforcing **user-declared policies**
- Providing **observable correctness metrics**

The Market layer **never emits raw protocol messages**.

---

## 7. Time Model

Market streams operate across multiple time domains:

- **Exchange Time** – timestamp provided by the exchange
- **Arrival Time** – time of ingestion by the SDK
- **Delivery Time** – time of callback invocation

Ordering, latency, and batching policies explicitly select which
time domain governs stream behavior.

Each Market stream selects a primary time domain for ordering guarantees;
other domains remain observable but non-authoritative.

---

## 8. Policy-Driven Design

Market streams are configured via explicit policies.

Policies fall into two orthogonal categories:

### Correctness Policies
- Ordering guarantees
- Gap tolerance
- Snapshot alignment rules

### Liveness Policies
- Stall on violation
- Drop data
- Resynchronize
- Terminate stream

Policies are:
- Declarative
- Explicit
- Evaluated at stream construction time

---

## 9. Example Conceptual API (Non-Final)

```cpp
auto trades =
    market.trades("BTC/USD")
          .consistency(strict)
          .liveness(stall)
          .ordering(exchange_time);

trades.on_update([](const Trade& t) {
    process(t);
});
```

This API expresses *intent*, not protocol wiring.

---

## 10. Dependency Rules (Strict)

The following rules **must never be violated**:

- Market **may include Core**
- Lite **may include Core**
- Core **must never include Market or Lite**
- Lite **must never include Market**
- Market logic **must never drive Core API changes for convenience**

---

## 11. State Management Model

Each Market stream owns:

- Its own state machine
- Its own replay buffer (if enabled)
- Its own gap detection logic
- Its own lifecycle

State is:
- Isolated per stream
- Deterministic
- Explicitly resettable

---

## 12. Error Handling Philosophy

Errors are **semantic**, not transport-level.

Examples:
- Gap detected
- Snapshot mismatch
- Replay failure

Market streams may:
- Stall
- Resynchronize
- Escalate
- Terminate

All behavior is governed by policy.

---

## 13. Observability

Market streams expose:

- Message counts
- Gap counters
- Replay counts
- Snapshot latency
- End-to-end delivery latency

Observability is:
- Optional
- Zero-cost when disabled
- Read-only to consumers

---

## 14. Non-Goals

The Market layer explicitly does **not** aim to:

- Replace Core APIs
- Hide protocol reality from advanced users
- Be exchange-agnostic
- Optimize for minimal binary size
- Maximize throughput at the expense of semantic correctness

---

## 15. Rationale

This architecture ensures:

- Core remains honest and minimal
- Lite remains portable
- Market remains powerful, correct, and explicit

It reflects how production trading infrastructure is structured and
prevents the most common SDK design pitfalls.

**Notes:**

- The prohibition on dependencies on Lite prevents Lite convenience
  abstractions from constraining Market correctness or policy expression.

- Not all Market streams require long-lived state (e.g. trades), but all
  Market streams enforce semantic guarantees.

- Market streams may internally maintain state, but only expose immutable,
  ephemeral views to consumers. State is never shared with or mutated by
  user code.

---

## 16. Status

This document defines the **architectural contract** of the Market layer.

Implementation must conform to this document.

---

## Related Documents

➡️ **[Audience Contract](./AUDIENCE_CONTRACT.md)**

➡️ **[Public API Stability](./PUBLIC_API_STABILITY.md)**

➡️ **[Quick Usage Guide](./QUICK_USAGE.md)**

---

⬅️ [Back to README](./README.md#architecture)
