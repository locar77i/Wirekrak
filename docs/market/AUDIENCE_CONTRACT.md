# Wirekrak Market — Audience Contract

## Purpose

**Wirekrak Market exists to serve users for whom correctness is a primary requirement,
not an incidental property.**

It defines a semantic boundary above protocol-level data and below application logic,
ensuring that users can build trading and analytical systems without accidentally
violating market data correctness constraints.

Market is not a convenience layer, and it is not a replacement for Core.
It is a **semantic authority** over market data interpretation.

This contract defines what Market **is**, **is not**, and **must never become**.

---

## Intended Audience

Wirekrak Market is intended for:

- Trading systems and execution engines
- Quantitative research pipelines
- Risk, surveillance, and analytics systems
- Product infrastructure that depends on correct market state
- Engineers who need explicit correctness and liveness guarantees

Market is **not** intended for:

- Casual data consumption
- Rapid prototyping without correctness requirements
- Ultra-low-latency protocol experimentation
- Exchange-agnostic SDK consumers
- Users unwilling to reason about correctness tradeoffs

Those users should use **Wirekrak Lite**.

Ultra-low-level protocol control belongs in **Wirekrak Core**.

---

## Scope of Responsibility

Market exposes **semantic market streams**, not protocol messages and not raw domain observations.

### Market provides:
- Stateful market abstractions (trades, order books, tickers)
- Snapshot–delta coordination and validation
- Replay and resynchronization mechanisms
- Explicit correctness policies
- Explicit liveness policies
- Deterministic, documented behavior under failure
- Observability into correctness and delivery

### Market does NOT provide:
- Raw protocol access
- Exchange-agnostic abstractions
- Implicit or “magic” correctness
- Best-effort convenience defaults
- Guarantees that are not explicitly declared

Market assumes users are willing to make **informed tradeoffs**.

---

## Guarantees & Non-Guarantees

### Market guarantees:
- Correctness semantics consistent with declared policies
- Clear failure modes under correctness violations
- Explicit ownership of snapshot–delta validation
- Deterministic state transitions
- Stable semantic stream interfaces

### Market does NOT guarantee:
- Zero latency
- Maximum throughput
- Lossless delivery under all failure modes
- Exchange portability
- Correctness without policy declaration

Correctness is **opted into explicitly**, never assumed.

---

## Correctness vs Liveness

Market recognizes that **correctness and liveness are in tension**.

Users must explicitly choose how violations are handled:

- Stall
- Resynchronize
- Drop data
- Terminate streams

Market will never silently choose on the user’s behalf.

If no policy is declared, Market defaults to **conservative correctness**, not availability.

---

## Architectural Constraints

The following constraints are **non-negotiable**:

1. Market depends on Core; Core must never depend on Market
2. Market must not depend on Lite
3. Market must not introduce protocol behavior into Core
4. Market must not exist as a façade over Lite
5. Market must remain explicit and policy-driven
6. Market must be allowed to reject unsafe usage

Violating these constraints invalidates the Market layer.

---

## Feature Admission Rules

A feature may be added to Market **only if all of the following are true**:

- It represents a real market semantic (not protocol trivia)
- It requires stateful reasoning or validation
- It cannot be safely implemented in Lite
- It has clearly defined correctness semantics
- Its failure modes can be explicitly documented

If a feature is purely protocol-level, it belongs in **Core**.
If a feature is convenience-oriented, it belongs in **Lite**.

---

## Relationship to Other Layers

### Relationship to Core

Core is the **source of protocol truth**.

Market:
- Consumes Core primitives
- Builds semantic state machines on top
- Never alters Core behavior
- Never drives Core evolution for convenience

### Relationship to Lite

Lite is a **domain façade**.
Market is a **semantic authority**.

They serve different audiences and must remain independent.

---

## Evolution Policy

Market is allowed to evolve **carefully but assertively**.

- Breaking changes are acceptable with major version bumps
- Semantic clarity takes precedence over backward compatibility
- Incorrect APIs may be removed
- Performance optimizations must not weaken semantics

Market is allowed to be strict.
Market is allowed to say “no”.

---

## Final Principle

> **Market exists to make incorrect systems hard to build,  
> even when incorrect systems would be easier or faster.**

This principle defines Wirekrak Market.

---

## Related Documents

➡️ **[Architecture Overview](./ARCHITECTURE.md)**

➡️ **[Audience Contract](./AUDIENCE_CONTRACT.md)**

➡️ **[Public API Stability](./PUBLIC_API_STABILITY.md)**

---

⬅️ [Back to README](./README.md#audience-contracts)
