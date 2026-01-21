## Audience Contract <a name="audience-contract"></a>

**Wirekrak Lite** is designed for application developers and data consumers
who wantaccess to domain-level market data without dealing with protocol mechanics
or infrastructure concerns.

Lite provides a minimal, conservative, exchange-agnostic API with safe defaults
and explicitly limited guarantees. It deliberately restricts access to low-level
concepts in order to reduce cognitive load and misuse. The scope, guarantees, and
limitations of Lite are formally defined in the Lite Audience Contract.

➡️ **[Audience Contract](./AUDIENCE_CONTRACT.md)**

---

## Architecture <a name="architecture"></a>

**Wirekrak Lite** is implemented as a thin, conservative façade over the Wirekrak Core
protocol layer. It consumes Core primitives to expose stable, domain-level market data
streams while deliberately avoiding ownership of protocol state, lifecycle management,
or recovery semantics. Lite does not introduce independent behavior or correctness
guarantees beyond what is explicitly documented; instead, it restricts access to
low-level concepts to reduce misuse and cognitive load. This design ensures Lite
remains exchange-agnostic, portable, and safe, while allowing Core to evolve
independently to support advanced and correctness-critical use cases.

➡️ **[Architecture Overview](./ARCHITECTURE.md)**

---

## Public API Stability <a name="api-stability"></a>

**Wirekrak Lite** follows a strict public API stability policy. All symbols exposed
through the Lite public surface are guaranteed to remain source- and binary-compatible
within the same major version.

This document defines exactly which parts of the Lite API are stable, what evolution
is allowed, and what is explicitly *not* guaranteed.

➡️ **[Public API Stability](./PUBLIC_API_STABILITY.md)**

---

## ⚡ Getting Started <a name="quick-usage"></a>

The Quick Usage Guide provides a fast, practical introduction to the **Wirekrak Lite** API.
It is intended for users who want to subscribe to trades or order books with minimal setup
and without understanding internal architecture.

This guide assumes no prior knowledge of Wirekrak internals and focuses on the most common
usage patterns.

➡️ **[Getting Started Guide](./QUICK_USAGE.md)**

---

⬅️ [Back to README](../ARCHITECTURE.md#wirekrak-lite)
