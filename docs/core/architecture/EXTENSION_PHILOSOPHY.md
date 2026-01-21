# Wirekrak Extension Philosophy

Wirekrak is designed as **infrastructure**, not a framework.

Its extensibility model prioritizes **correctness, determinism, and long-term evolvability** over ad-hoc customization. As a result, Wirekrak exposes **architectural extension seams** rather than user-level hooks or plugin registries.

This document explains what that means in practice.

---

## 1. Extension by Composition, Not Customization

Wirekrak favors **composition of replaceable components** over runtime modification of internal behavior.

Instead of exposing callbacks deep inside execution flow, Wirekrak allows consumers to:
- replace entire layers,
- compose alternative implementations,
- extend functionality by adding new components that respect existing invariants.

This approach ensures that extensions do not compromise:
- message ordering,
- state consistency,
- deterministic recovery,
- protocol correctness.

---

## 2. Clear Separation Between Extension Seams and Invariants

Not all parts of the system are equally extensible.

Wirekrak explicitly distinguishes between:

### Extension Seams
Layers intended to be replaced or extended:
- Transport backends
- Stream (connection policy) implementations
- Protocol implementations (e.g. Kraken)
- Channel definitions
- Replay and recovery strategies
- Persistence and auditing (WAL)

### Invariant Boundaries
Layers that enforce correctness and must not be casually modified:
- State machines
- Ordering guarantees
- Deterministic replay semantics
- Schema validity

This distinction is intentional: extensibility must not undermine correctness.

---

## 3. Infrastructure-Level Extensibility

Wirekrak’s extension points operate at the **infrastructure level**, not the application level.

This enables:
- Adding new venues or protocol versions
- Supporting alternative transports or network environments
- Customizing reconnection and liveness policy
- Implementing deterministic simulation or replay
- Introducing persistence, auditing, or offline analysis

These extensions are structural and long-lived, rather than ad-hoc or request-scoped.

---

## 4. Policy Is Isolated from Mechanism

Wirekrak separates **policy** from **mechanism** wherever possible.

Examples:
- Transport handles byte delivery, not protocol semantics
- Stream layer governs lifecycle and reconnection, not message meaning
- Protocol layers define message structure and behavior, not transport mechanics

This allows policy to evolve independently of protocol or transport without cascading changes.

---

## 5. Protocols Are Namespaced and Explicit

Protocol implementations are explicitly namespaced (e.g. `protocol/kraken`).

This ensures:
- Clear ownership of protocol-specific assumptions
- Safe coexistence of multiple protocols or venues
- Incremental evolution of schemas and parsers
- Isolation of protocol changes from core infrastructure

Adding a new protocol is an additive operation, not a rewrite.

---

## 6. Determinism Is a First-Class Constraint

Some extension points are intentionally constrained to preserve determinism.

For example:
- Replay and recovery logic enforces ordering guarantees
- State transitions are explicit and validated
- Extensions must obey existing lifecycle rules

This is not a limitation — it is a design principle.  
Wirekrak values **predictable behavior under failure** over maximum flexibility.

---

## 7. No Implicit Plugin System by Design

Wirekrak does not provide:
- runtime plugin registries
- generic middleware chains
- unrestricted user callbacks inside core execution paths

These mechanisms tend to:
- obscure control flow,
- weaken invariants,
- complicate reasoning under failure.

Instead, Wirekrak encourages extensions to be **explicit, compositional, and testable**.

---

## 8. Public Facades Are Intentionally Thin

The public entry points are intentionally minimal.

Wirekrak is designed to be **embedded**, not executed.  
Higher-level ergonomics and opinionated APIs belong in facade layers (e.g. *Wirekrak Lite*), not in the core.

This separation allows:
- the core to remain stable and correct,
- facades to evolve independently,
- different consumption styles without compromising invariants.

---

## 9. Extensibility Is Proven Through Tests

Replaceability is validated through:
- mock transports,
- isolated protocol tests,
- deterministic replay scenarios.

Tests serve as living documentation of extension boundaries.

---

## 10. Summary

Wirekrak is extensible by design — but deliberately so.

It prioritizes:
- explicit boundaries over convenience hooks,
- composition over mutation,
- determinism over dynamic behavior.

This philosophy aligns Wirekrak with long-lived infrastructure systems rather than short-term frameworks.

---

⬅️ [Back to README](../ARCHITECTURE.md#extensibility)
