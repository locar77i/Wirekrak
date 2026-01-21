# Wirekrak Architecture

This document defines the **system-level architecture** of **Wirekrak**.

It describes:
- The major architectural layers
- Their responsibilities and intended audiences
- Dependency and evolution rules
- Correctness and failure boundaries

This document does **not** describe internal implementation details of any single layer.
Layer-specific architecture is documented separately.

---

## Architectural Intent

Wirekrak is designed as **infrastructure**, not as a convenience SDK.

Its architecture prioritizes:

- **Deterministic behavior under failure**
- **Explicit lifecycle and recovery semantics**
- **Schema-first, strongly typed protocols**
- **Clear separation of responsibility**
- **Long-term evolvability without semantic drift**

Wirekrak intentionally trades ease of use and implicit behavior for correctness,
clarity, and long-term architectural discipline.

---

## Design Goals

Wirekrak is built around the following system-level goals:

- Deterministic behavior across reconnects and partial failures
- Explicit modeling of lifecycle and failure states
- Separation of protocol truth from market semantics
- Clear audience targeting for each API layer
- Independence of evolution between layers
- Suitability for ultra-low-latency and infrastructure use cases

These goals apply to the **system as a whole**, not to any single API.

---

## High-Level Layered Architecture

Wirekrak is organized as a set of **strict, intentionally incomplete layers**
with explicit dependency direction and well-defined responsibility boundaries.

```
                    Application Code
            (trading systems, analytics, apps)
                            ▲
            ┌───────────────┴─────────────────────┐
        Market API                             Lite API
  (semantic correctness)                (exchange-agnostic SDK)
            ▲                                     ▲
            └───────────────┬─────────────────────┘
                            │
                         Core API
                     (protocol truth)
                            ▲
                            │
                    Core Stream Layer
                 (Reconnect / liveness)
                            ▲
                            │
                        Transport
                (WebSocket / IO backends)
```

### Dependency Rules

- **Core** is the single source of protocol truth
- **Lite** depends on Core
- **Market** depends on Core
- **Lite and Market are peers** and must not depend on each other
- **Core must never depend on higher layers**
- Dependency direction must never be reversed

Violating these rules is considered an architectural error.

```Important
Core is a header-only infrastructure engine and the single source of protocol truth.
Lite and Market are peer APIs built on Core for different audiences.
Applications should depend on Lite or Market based on correctness requirements,
and use Core directly only when full protocol control or ultra-low-latency behavior is required.
```

---

## Layer Responsibilities

### Wirekrak Core

**Role:** Protocol truth and foundational infrastructure layer

**Properties:**
- Header-only
- Allocation-free on hot paths
- Transport-agnostic
- Poll-driven execution
- Designed for ultra-low-latency (ULL) systems

Core is responsible for:
- Exchange protocol semantics
- Lifecycle state machines
- Transport abstraction
- Deterministic reconnection behavior
- Protocol correctness and validation

Core does **not** encode market semantics or correctness guarantees beyond
what is explicitly present in the exchange protocol.

Core is free to evolve aggressively as long as its invariants are preserved.

➡️ **[Core API Overview](core/README.md)**  

---

### Wirekrak Lite

**Role:** Conservative, exchange-agnostic SDK facade built directly on top of Wirekrak Core.

**Properties:**
- Compiled library
- Stable public API
- DTO-based interfaces
- Higher-level abstractions
- Zero protocol knowledge required

Lite is responsible for:
- Simplified, stable client APIs
- Exchange-neutral domain DTOs
- Reduced cognitive load for application developers
- Explicit but limited guarantees

Lite does **not**:
- Own protocol lifecycle
- Provide replay or recovery semantics
- Enforce market correctness

➡️ **[Lite API Overview](lite/README.md)**  

---

### Wirekrak Market

**Role:** Semantic market correctness layer built directly on top of Wirekrak Core.

**Properties:**
- Compiled library
- Stable semantic stream interfaces
- Stateful market abstractions
- Explicit correctness and liveness policies
- Exchange-specific semantics

Market is responsible for:
- Stateful market abstractions (trades, books, etc.)
- Snapshot–delta coordination and validation
- Replay and resynchronization semantics
- Explicit correctness and liveness policies
- Observability into semantic correctness

Market is the **sole authority** for market semantics.
It intentionally trades convenience and portability for explicit guarantees.

Market does **not** replace Core or Lite, and does not depend on Lite.

➡️ **[Market API Overview](market/README.md)**  

---

## Correctness & Failure Boundaries

Each layer enforces correctness within a **clearly scoped boundary**:

| Layer  | Correctness Scope |
|------|------------------|
| Core | Protocol correctness and lifecycle validity |
| Lite | Safe, conservative domain observation |
| Market | Semantic market state correctness |

No layer compensates for correctness violations in another.
Completeness is achieved by **choosing the correct layer**, not by layering convenience.

---

## Audience Separation

Wirekrak intentionally serves **different users with different APIs**:

| Audience | Recommended Layer |
|--------|-------------------|
| Infrastructure / ULL engineers | Core |
| Application developers | Lite |
| Trading & analytics systems | Market |

Using a layer outside its intended audience is considered a design mistake.

---

## Evolution Model

Wirekrak layers evolve **independently**:

- Core may evolve aggressively to match protocol reality
- Lite evolves conservatively with stable APIs
- Market evolves deliberately as semantics are refined

Higher layers must never constrain the evolution of lower layers.

Architectural clarity takes precedence over backward compatibility.

---

## Extensibility Philosophy

Wirekrak is extensible by **composition**, not by runtime customization.

Extension occurs by:
- Adding new transports
- Adding new protocol layers
- Adding new semantic market abstractions

Wirekrak deliberately avoids:
- Plugin systems
- Implicit hooks
- Runtime mutation of behavior

This preserves determinism, testability, and correctness.

---

## What This Document Is Not

This document does **not** describe:
- Threading models
- Parser internals
- Dispatcher mechanics
- Replay algorithms
- Telemetry implementation details

Those topics belong to **layer-specific architecture documents**.

---

## Summary

Wirekrak’s architecture is intentionally layered, explicit, and conservative.

Each layer is incomplete by design.
Correctness is achieved by **selecting the appropriate layer**, not by hiding complexity.

This structure allows Wirekrak to evolve as serious infrastructure while remaining
understandable, testable, and correct.

---

⬅️ [Back to README](../README.md#architecture)
