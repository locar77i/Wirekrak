# Wirekrak Architecture

This document describes the architectural design, guiding principles, and internal structure of **Wirekrak**.

Wirekrak is designed as **infrastructure**, not a convenience wrapper. Its architecture prioritizes determinism, correctness, explicit lifecycle management, and long-term evolvability over ad-hoc flexibility.

---

## Design Goals

Wirekrak is built around the following core goals:

- **Deterministic behavior** under failure and reconnection
- **Explicit lifecycle management** (no hidden threads or runtimes)
- **Schema-first, strongly typed protocols**
- **Clear separation of concerns** across layers
- **Ultra-low-latency compatibility** for infrastructure use cases
- **Long-term API survivability**

These goals drive every architectural decision in the system.

---

## High-Level Layered Architecture

Wirekrak is organized into a strict, one-way layered architecture:

```
  Application Code     ← Examples, SDK users, apps
        ▲
   Wirekrak Lite       ← Compiled SDK library (facade)
        ▲
   Wirekrak Core       ← Header-only, ULL infrastructure
```

- Dependencies flow strictly **upward**
- Core never depends on Lite
- Lite depends on Core
- Applications should depend on Lite unless ultra-low-latency constraints require Core directly

```Important``` Core is a header-only infrastructure engine. Lite is the supported SDK surface. Applications should depend on Lite unless ultra-low-latency constraints require otherwise.

---

## Core vs Lite

### Wirekrak Core <a name="wirekrak-core"></a>

Wirekrak Core is the foundational infrastructure layer.

**Properties:**
- Header-only
- Allocation-free on hot paths
- Transport-agnostic
- Poll-driven execution
- Designed for ultra-low-latency (ULL) systems

Core provides:
- WebSocket stream lifecycle management
- Liveness detection primitives
- Protocol-agnostic transport contracts
- Deterministic state machines

Core is free to evolve aggressively as long as its invariants are preserved.

➡️ **[Core API overview](architecture/core/README.md)**

---

### Wirekrak Lite <a name="wirekrak-lite"></a>

Wirekrak Lite is the **stable, user-facing SDK facade** built on top of Wirekrak Core, designed for rapid integration, hackathons, and production trading systems.

**Properties:**
- Compiled library
- Stable public API
- DTO-based interfaces
- Higher-level abstractions
- Zero protocol knowledge required

Lite provides:
- Simplified client APIs
- Stable DTOs
- Clear error reporting
- SDK-level semantics

Lite is the supported integration surface for applications.

➡️ **[Lite API overview](./lite/README.md)**

---

### Threading Model <a name="threading-model"></a>

Wirekrak Core currently uses a deliberate **2-thread architecture** optimized for
low latency and correctness. A 3-thread model (dedicated parser thread) is
planned but intentionally postponed until full benchmarking is completed.

➡️ **[Read the full threading model rationale](architecture/core/ThreadingModel.md)**

---

## Transport Layer <a name="transport"></a>

The transport layer abstracts WebSocket connectivity independently of any protocol.

**Key characteristics:**
- Modeled using C++20 concepts
- Protocol-agnostic interfaces
- Explicit error signaling
- Fully mockable for testing

The current implementation uses Windows WinHTTP WebSocket APIs, but the architecture allows alternative transports (Boost.Asio, libwebsockets, etc.) to be added without impacting protocol logic.

➡️ **[Transport Overview](architecture/core/transport/Overview.md)**

---

## Stream Client <a name="stream-client"></a>

The Stream Client manages WebSocket lifecycle independently from any exchange protocol.

Responsibilities include:
- Connection establishment and teardown
- Dual-signal liveness detection (heartbeats + message flow)
- Deterministic reconnection logic
- Explicit state transitions

The Stream Client is **poll-driven** and does not spawn background threads.
On reconnect, transports are fully destroyed and recreated to avoid undefined internal state.

This layer is reusable across exchanges and protocols.

➡️ **[Stream Client Overview](architecture/core/stream/Client.md)**

---

## Kraken Protocol Layer  <a name="kraken-session"></a>

The Kraken protocol layer adapts the generic stream client to the Kraken WebSocket v2 API. It exposes a protocol-oriented,
type-safe API for subscribing to Kraken channels while fully abstracting transport, liveness, and reconnection concerns.
It uses an ACK-based subscription model with automatic replay on reconnect, explicit polling,
and strongly typed message dispatch. This design enables deterministic behavior and clean integration
with trading engines, analytics pipelines, and real-time systems.

Responsibilities include:
- Typed subscription requests
- ACK-based subscription lifecycle tracking
- Message routing by channel and symbol
- Protocol correctness validation

Protocol handling is **exchange-specific** and intentionally isolated from transport and stream logic.

➡️ **[Kraken Client Overview](architecture/core/protocol/kraken/Session.md)**

---

## Schema-First Message Design

All Kraken messages are modeled using a schema-first approach.

**Benefits:**
- Strongly typed request, response, and event models
- Compile-time validation of message shapes
- No string-based JSON access in application code
- Clear separation between schema and transport

Schemas define *what* a message is, not *how* it is transported.

---

## Parser Architecture  <a name="parser"></a>

Wirekrak includes a production-grade parser designed for real-time market data. It uses schema-strict validation, constexpr-based enum decoding, and zero-allocation parsing on top of `simdjson`.

The architecture cleanly separates routing, parsing, and domain adaptation, enforcing real exchange semantics (e.g. snapshot vs update invariants) and rejecting malformed messages deterministically.

```
Router → Parsers → Adapters → Helpers
```

- **Router**: Routes messages by channel/method
- **Parsers**: Handle control flow and protocol semantics
- **Adapters**: Convert primitives to domain types (enums, symbols, timestamps)
- **Helpers**: Validate JSON structure and extract primitives

### Parser Router

Incoming WebSocket messages are first routed by method/channel to the appropriate message parser, ensuring each payload is handled by the correct protocol-specific logic with minimal branching.

### Layered Parsers (Helpers → Adapters → Parsers)

Low-level helpers validate JSON structure and extract primitives, adapters perform domain-aware conversions (enums, symbols, timestamps), and message parsers handle control flow and logging. This separation keeps parsing fast, safe, and maintainable.

### Error Semantics

Parsing distinguishes between:
- Invalid schema
- Invalid values

This allows robust handling of real-world API inconsistencies while remaining allocation-free and exception-free.

```note``` Every parser is fully unit-tested against invalid, edge, and protocol-violating inputs, making refactors safe and correctness provable.

➡️ **[Parser Overview](architecture/core/protocol/kraken/Parser.md)**

---

## Dispatcher <a name="distpatcher"></a> 

The Dispatcher delivers decoded protocol events to consumers.

**Characteristics:**
- Compile-time channel traits
- Symbol interning
- Zero runtime channel branching
- Multiple listeners per symbol

The dispatch path is designed to be extremely fast and deterministic.

➡️ **[Dispatcher Overview](architecture/core/protocol/kraken/Dispatcher.md)**

---

## Subscription Manager <a name="subscription-manager"></a>

The Subscription Manager tracks the full lifecycle of Kraken subscriptions.

Responsibilities:
- Track pending subscriptions
- Track active subscriptions
- Track pending unsubscriptions
- Transition states only after explicit ACKs

Kraken’s multi-symbol subscription model is handled safely and deterministically, including partial acknowledgements.

➡️ **[Subscription Manager Overview](architecture/core/protocol/kraken/SubscriptionManager.md)**

---

## Subscription Replay <a name="subscription-replay"></a>

The Replay module records confirmed subscriptions and replays them automatically after reconnects.

This ensures:
- Continuity after transient network failures
- No duplicate or invalid subscriptions
- Deterministic recovery behavior

Replay is protocol-aware but transport-agnostic.

➡️ **[Subscription Replay Overview](architecture/core/protocol/kraken/SubscriptionReplay.md)**

---

## Telemetry <a name="telemetry"></a>

Wirekrak provides **compile-time, zero-overhead telemetry** designed for low-latency and infrastructure-grade systems.

Telemetry is:
- observational only (never affects behavior)
- fully removable at compile time
- safe for latency-critical paths

### Telemetry Levels

| Level | Description | Default |
|------|------------|---------|
| **L1** | Mechanical metrics (bytes, messages, errors, message shape) | **ON** |
| **L2** | Diagnostic metrics (deeper timing and pressure insight) | OFF |
| **L3** | Analytical metrics (profiling, tracing, experimentation) | OFF |

Higher levels automatically enable lower ones.

### Enabling Telemetry

```bash
-DWIREKRAK_ENABLE_TELEMETRY_L1=ON
-DWIREKRAK_ENABLE_TELEMETRY_L2=OFF
-DWIREKRAK_ENABLE_TELEMETRY_L3=OFF
```
When disabled, telemetry is completely compiled out.

➡️ **[Telemetry Level Policy](architecture/core/TelemetryLevelPolicy.md)**

---

### Low-latency Common Resources (`lcr`)

Wirekrak includes a small internal utility layer named **LCR** (Low-latency Common Resources).

`lcr` contains reusable, header-only building blocks designed for
low-latency systems and used across ULL systems, including:

- lock-free data structures
- lightweight `optional` and helpers
- bit-packing utilities
- logging abstractions

These utilities are **domain-agnostic** (not Kraken-specific) and are intentionally kept separate
from the Wirekrak protocol code to allow reuse across other ULL (Ultra-Low Latency) projects.

Wirekrak itself depends on `lcr`, but `lcr` does not depend on Wirekrak.

---

## Extensibility Philosophy  <a name="extensibility"></a>

Wirekrak is extensible by **composition**, not by ad-hoc customization.

Extension is achieved by:
- Replacing transport implementations
- Replacing protocol layers
- Extending schema definitions
- Adding new exchanges

Wirekrak deliberately avoids:
- Plugin APIs
- Runtime hooks
- Implicit extension points

This preserves determinism, testability, and correctness.

➡️ **[Extension Philosophy](architecture/core/ExtensionPhilosophy.md)**

---

## What Wirekrak Is Not

Wirekrak is explicitly **not**:

- A generic async framework
- A plugin-based system
- A JSON-stream wrapper
- A callback-driven runtime
- A UI-oriented SDK

It is infrastructure designed for serious real-time systems.

---

## Future Directions

Planned and possible future work includes:

- Additional transport backends
- Multi-exchange protocol support
- Deterministic write-ahead logging
- Offline replay and simulation pipelines
- Further latency benchmarking and tuning

These evolutions are designed to occur **behind stable API boundaries**.

---

## Summary

Wirekrak’s architecture is intentionally conservative, explicit, and layered.
It trades convenience for correctness and long-term survivability, making it suitable as a foundation for production-grade real-time trading and market-data systems.

---

⬅️ [Back to README](../README.md#architecture)