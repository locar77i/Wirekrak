# Wirekrak Core — Audience Contract

## Purpose

**Wirekrak Core exists to provide explicit, correct, and high-performance access to exchange protocols.**
It is the foundation of Wirekrak and the single source of truth for protocol semantics, lifecycle, and
protocol correctness.

Core is not designed to optimize for ease of use.
It is designed to optimize for **control, transparency, and correctness**.

This contract defines who Core is for, what it guarantees, and what responsibilities it places on its users.

---

## Intended Audience

Wirekrak Core is intended for:

- Infrastructure engineers
- Systems programmers
- Quantitative developers
- Exchange and protocol integrators
- Users building correctness-critical pipelines

Core users are expected to:
- Read documentation and headers
- Reason about lifecycle and failure modes
- Understand exchange-specific semantics
- Accept explicitness and cognitive load

Core is **not** intended for:
- Casual application development
- Rapid prototyping without protocol knowledge
- Users who want implicit guarantees or hidden behavior

---

## Scope of Responsibility

Core exposes **protocol-level behavior**, not simplified domain abstractions.

### Core provides:
- Explicit protocol and exchange semantics
- Lifecycle state machines
- Connection and subscription control
- Failure and error propagation
- Performance-oriented abstractions
- Low-level building blocks for higher layers

### Core does NOT provide:
- Implicit recovery or replay guarantees
- Automatic correctness beyond what is explicitly modeled
- Domain-level simplifications
- Protection from misuse

Correct usage of Core requires deliberate design and understanding by the user.

Market-level correctness (e.g. snapshot–delta validation, replay semantics, or state reconstruction)
is explicitly outside the scope of Core.

---

## Guarantees & Non-Guarantees

### Core guarantees:
- Faithful representation of exchange protocol behavior
- Explicit and observable lifecycle transitions
- Transparent failure signaling
- No hidden retries or silent recovery
- Predictable performance characteristics

### Core does NOT guarantee:
- Ease of use
- Safety against misuse
- Exchange-agnostic behavior
- Backward compatibility without notice
- Stable abstractions above the protocol layer

If stronger safety or domain abstraction is required, a higher-level API should be used.

---

## Architectural Principles

The following principles define Wirekrak Core:

1. **Explicitness over convenience**
2. **Correctness over ergonomics**
3. **Transparency over abstraction**
4. **Control over automation**
5. **Performance over generality**

Core intentionally exposes sharp edges.
These edges are part of its design contract.

---

## Evolution Policy

Core is allowed to evolve aggressively.

- Breaking changes are acceptable when required for correctness
- APIs may change to reflect protocol reality
- New abstractions must justify their complexity
- Backward compatibility is secondary to semantic accuracy

Core must never be constrained by higher-level ergonomics.

---

## Relationship to Higher-Level APIs

Wirekrak Core may be wrapped or adapted by higher-level APIs.
Such layers must:

- Depend on Core without altering its semantics
- Restrict or simplify behavior without inventing guarantees
- Treat Core as the authoritative source of truth

Core itself remains independent and self-contained.

---

## Final Principle

> **Wirekrak Core exposes reality, not convenience.  
> Users of Core accept responsibility for correctness and failure handling.**

This principle governs all Core design decisions.

---

⬅️ [Back to README](./README.md#audience-contract)
