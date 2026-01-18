# Wirekrak Lite — Audience Contract

## Purpose

**Wirekrak Lite exists to serve a different audience than Wirekrak Core.**
It is not a convenience layer, a UX patch, or a subset of Core.

Lite defines a **conceptual boundary** between:
- **Domain consumers** of market data
- **Protocol engineers** managing correctness and lifecycle

This contract describes what Lite **is**, **is not**, and **will never become**.

---

## Intended Audience

Wirekrak Lite is intended for:

- Application developers
- Data consumers
- Rapid prototyping and integration use cases
- Users who want domain-level market data streams
- Users who should not reason about protocol machinery

Lite is **not** intended for:

- Infrastructure engineers
- Protocol implementers
- Low-latency system tuning
- Exchange-specific customization
- Correctness-critical pipeline construction

Those users must use **Wirekrak Core**.

---

## Scope of Responsibility

Lite exposes **domain-level observations**, not protocol behavior.

### Lite provides:
- Streams of trades, quotes, and book levels
- Stable, exchange-agnostic domain DTOs
- Safe, opinionated defaults
- A minimal, conservative API surface
- Deterministic, transparent behavior

### Lite does NOT provide:
- Protocol sessions
- Lifecycle state machines
- Recovery semantics
- Replay guarantees
- Durability guarantees
- Ordering guarantees beyond what is explicitly documented

Lite does not attempt to hide failures.
If correctness cannot be guaranteed, Lite will surface errors or terminate streams.

---

## Guarantees & Non-Guarantees

### Lite guarantees:
- Domain-level abstraction over market data
- No exposure to exchange-specific protocol details
- No requirement to understand Core internals
- Conservative behavior with explicit limitations

### Lite does NOT guarantee:
- Message completeness
- Data durability
- Lossless recovery
- Exact ordering across reconnects
- Exchange-specific feature parity

If such guarantees are required, **Core must be used**.

---

## Architectural Constraints

The following constraints are **non-negotiable**:

1. Lite depends on Core; Core must never depend on Lite
2. Lite is a façade; it must not implement independent behavior
3. Lite may restrict capabilities but must not invent semantics
4. Lite must remain smaller than Core in surface and complexity
5. Lite must remain exchange-agnostic at all times

Violating any of these constraints invalidates Lite’s purpose.

---

## Feature Admission Rules

A feature may be added to Lite **only if all of the following are true**:

- It can be supported consistently across exchanges
- It does not require protocol-level reasoning
- It does not expose lifecycle or recovery mechanics
- It can be documented without reference to exchange internals
- It reduces user risk rather than increasing it

If any condition fails, the feature belongs in **Core only**.

---

## Relationship to Wirekrak Core

Wirekrak Core is the **source of truth** for:
- Protocol correctness
- Exchange-specific behavior
- Lifecycle management
- Performance tuning
- Recovery and durability semantics

Lite exists to **limit exposure** to those concerns, not replace them.

Lite must never compensate for poor Core design or documentation.

---

## Evolution Policy

Lite is intentionally conservative.

- Breaking changes in Lite are avoided whenever possible
- Features are added slowly and deliberately
- Removal of Lite is acceptable if it no longer serves its audience
- Core evolution must never be blocked by Lite ergonomics

Lite is allowed to be incomplete.
Lite is not allowed to be misleading.

---

## Final Principle

> **Lite protects users from concepts they should not need to understand.  
> Core empowers users who need full control.**

This distinction defines Wirekrak.

---

## Related Documents

➡️ **[Public API Stability](./PUBLIC_API_STABILITY.md)**

➡️ **[Quick Usage Guide](./QUICK_USAGE.md)**

---

⬅️ [Back to README](./README.md#audience-contract)