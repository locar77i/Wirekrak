# Wirekrak Core — Layer Architecture

This document describes the **internal architecture** of **Wirekrak Core**.

It defines how Core implements protocol correctness, lifecycle management,
and infrastructure-level guarantees. It does **not** describe system-level
layering or higher-level APIs such as Lite or Market.

---

## Scope & Responsibility

Wirekrak Core is the **protocol infrastructure layer** of the system.

Core is responsible for:
- Exchange protocol semantics
- Connection and subscription lifecycle
- Deterministic reconnection behavior
- Protocol-level correctness and validation
- Performance-critical infrastructure primitives

Core is **not** responsible for:
- Market semantics (e.g. snapshot–delta correctness)
- Replay of market state
- Domain-level abstractions
- User-facing convenience APIs

Those responsibilities belong to higher layers.

---

## Design Goals

Wirekrak Core is built around the following goals:

- **Deterministic behavior** under failure and reconnection
- **Explicit lifecycle management** (no hidden threads or runtimes)
- **Schema-first, strongly typed protocols**
- **Clear separation of concerns** within Core subsystems
- **Ultra-low-latency compatibility** for infrastructure use cases
- **Long-term protocol and infrastructure survivability**

These goals drive every architectural decision in Core.

---

## Threading Model <a name="threading-model"></a>

Wirekrak Core currently uses a deliberate **2-thread architecture** optimized for
low latency and protocol correctness.

A 3-thread model (dedicated parser thread) is planned but intentionally postponed
until full benchmarking and pressure analysis are completed.

➡️ **[Threading Model Rationale](./architecture/THREADING_MODEL.md)**

---

## Transport Layer <a name="transport"></a>

The transport layer abstracts WebSocket connectivity independently of any protocol.

**Key characteristics:**
- Modeled using C++20 concepts
- Protocol-agnostic interfaces
- Explicit error signaling
- Fully mockable for deterministic testing

The current implementation uses Windows WinHTTP WebSocket APIs, but alternative
transports (Boost.Asio, libwebsockets, etc.) can be added without impacting
protocol logic.

➡️ **[Transport Overview](./architecture/transport/Overview.md)**

---

## Transport Connection <a name="transport-connection"></a>

The Transport Connection manages WebSocket lifecycle independently from any exchange protocol.

Responsibilities include:
- Connection establishment and teardown
- Dual-signal liveness detection (heartbeats + message flow)
- Deterministic reconnection logic
- Explicit state transitions

The Transport Connection is **poll-driven** and does not spawn background threads.
On reconnect, transports are fully destroyed and recreated to avoid undefined state.

This subsystem is reusable across exchanges and protocols.

➡️ **[Stream Client Overview](./architecture/transport/connection/Overview.md)**

---

## Kraken Protocol Layer <a name="kraken-session"></a>

The Kraken protocol layer adapts the generic stream client to the Kraken WebSocket v2 API.

It exposes a protocol-oriented, type-safe API for subscribing to Kraken channels while
fully abstracting transport, liveness, and reconnection concerns.

The design uses:
- ACK-based subscription lifecycle tracking
- Explicit polling
- Strongly typed message dispatch
- Protocol-aware subscription replay

Responsibilities include:
- Typed subscription requests
- Subscription lifecycle tracking
- Message routing by channel and symbol
- Validation of protocol invariants

Protocol handling is **exchange-specific** and intentionally isolated.

➡️ **[Kraken Session Overview](./architecture/protocol/kraken/Session.md)**

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

Wirekrak Core includes a production-grade parser designed for real-time protocol handling.

It uses:
- Schema-strict validation
- `constexpr`-based enum decoding
- Zero-allocation parsing on top of `simdjson`

The architecture cleanly separates routing, parsing, and domain adaptation:

```
Router → Parsers → Adapters → Helpers
```

- **Router**: Routes messages by channel/method
- **Parsers**: Handle protocol control flow
- **Adapters**: Convert primitives to domain-safe types
- **Helpers**: Validate JSON structure and extract primitives

Parsing errors are classified as:
- Invalid schema
- Invalid values

Every parser is fully unit-tested against invalid, edge, and protocol-violating inputs.

➡️ **[Parser Overview](./architecture/protocol/kraken/Parser.md)**

---

## Subscription Manager <a name="channel-manager"></a>

The Subscription Manager tracks the full lifecycle of Kraken subscriptions.

Responsibilities:
- Track pending subscriptions
- Track active subscriptions
- Track pending unsubscriptions
- Transition states only after explicit ACKs

Kraken’s multi-symbol subscription model is handled deterministically, including partial
acknowledgements.

➡️ **[Subscription Manager Overview](./architecture/protocol/kraken/ChannelManager.md)**

---

## Subscription Replay <a name="subscription-replay"></a>

The Replay module records **confirmed protocol subscriptions** and replays them automatically
after reconnects.

This ensures:
- Continuity after transient network failures
- No duplicate or invalid subscriptions
- Deterministic protocol recovery

This is **protocol replay**, not market-state replay.

➡️ **[Subscription Replay Overview](./architecture/protocol/kraken/SubscriptionReplay.md)**

---

## Telemetry <a name="telemetry"></a>

Wirekrak provides **compile-time, zero-overhead telemetry** designed for infrastructure use.

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

➡️ **[Telemetry Level Policy](./architecture/TELEMETRY_LEVEL_POLICY.md)**

---

### Low-latency Common Resources (`lcr`)

Wirekrak includes a small internal utility layer named **LCR** (Low-latency Common Resources).

`lcr` contains reusable, header-only building blocks designed for low-latency systems and
used across ULL systems, including:

- lock-free data structures
- lightweight `optional` and helpers
- bit-packing utilities
- logging abstractions

These utilities are **domain-agnostic** (not Kraken-specific) and are intentionally kept separate
from the Wirekrak protocol code to allow reuse across other ULL (Ultra-Low Latency) projects.

Wirekrak itself depends on `lcr`, but `lcr` does not depend on Wirekrak.

---

## Extensibility Philosophy  <a name="extensibility"></a>

Wirekrak Core is extensible by **composition**, not by runtime customization.

Extension occurs by:
- Adding transports
- Adding protocol implementations
- Extending schemas
- Supporting new exchanges

Core deliberately avoids plugin systems or runtime hooks to preserve determinism.

➡️ **[Extension Philosophy](./architecture/EXTENSION_PHILOSOPHY.md)**

---

## What Wirekrak Core Is Not

Wirekrak Core is explicitly **not**:
- A generic async framework
- A plugin system
- A JSON-stream wrapper
- A callback-driven runtime
- A user-facing SDK

It is infrastructure for serious real-time systems.

---

## Summary

Wirekrak Core is a deterministic, protocol-correct infrastructure engine.

It exposes protocol reality explicitly, favors control over convenience,
and provides the foundation upon which higher-level APIs are safely built.

---

⬅️ [Back to README](./README.md#architecture)
