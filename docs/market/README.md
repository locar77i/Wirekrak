## Audience Contract <a name="audience-contract"></a>

**Wirekrak Market** is designed for trading systems, analytics pipelines, and product
infrastructure that require **correctness-aware, stateful market data**.

Market provides semantic market abstractions (such as trades and order books) with
**explicit correctness and liveness guarantees**, enforced through declarative policies.
Unlike Lite, Market does not aim to be exchange-agnostic or convenience-oriented.
It existsto ensure users cannot accidentally construct incorrect systems on top of valid
but incomplete protocol data.

The scope, guarantees, and non-goals of the Market API are formally defined in the
Market Audience Contract.

➡️ **[Audience Contract](./AUDIENCE_CONTRACT.md)**

---

## Architecture <a name="architecture"></a>

The Market layer is a semantic authority built directly on top of the Core protocol layer.
It owns snapshot–delta validation, replay logic, and market state machines, while remaining
independent from the Lite API.

The architectural design of the Market layer is defined prior to implementation and treated
as a binding contract.

➡️ **[Market Architecture](./ARCHITECTURE.md)**

---

## Public API Stability <a name="api-stability"></a>

**Wirekrak Market** follows a strict but explicit public API stability policy.

- Semantic stream interfaces and domain value types are stable within a major version
- Behavioral policies may evolve only via additive changes
- Breaking semantic changes require a major version bump

This document defines which parts of the Market API are stable, what evolution is permitted,
and which behaviors are intentionally *not* guaranteed.

➡️ **[Public API Stability](./PUBLIC_API_STABILITY.md)**

---

## ⚡ Getting Started <a name="quick-usage"></a>

The Market API is intended for users who are already familiar with market data concepts
and correctness tradeoffs.

The Getting Started guide introduces:
- Creating semantic market streams
- Declaring correctness and liveness policies
- Consuming stateful market data safely

➡️ **[Getting Started Guide](./QUICK_USAGE.md)**

---

⬅️ [Back to Architecture Overview](../ARCHITECTURE.md#wirekrak-market)
