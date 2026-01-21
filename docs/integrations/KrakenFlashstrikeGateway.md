# Kraken → Flashstrike Gateway

## Overview

This integration example demonstrates how **Wirekrak Lite** can be used as a real-time
market data ingestion layer feeding a **production-grade matching engine**.

The example connects to the **Kraken WebSocket API v2** via the Wirekrak Lite API,
subscribes to level-2 order book updates, normalizes prices and quantities, and injects
them directly into the Flashstrike matching engine for live processing.

This integration is intentionally built as an **opt-in experiment** and does not
affect Wirekrak Core, Wirekrak Market, or default Wirekrak builds.

---

## Architecture

The example is structured around a clear separation of responsibilities:

### Wirekrak Lite Client
- Manages WebSocket connection lifecycle
- Handles reconnection and liveness detection
- Exposes exchange-agnostic DTOs (`BookLevel`, `Trade`, etc.)
- Delivers incremental order book level updates via polling

> **Note:** This example intentionally uses the Lite API and does *not* rely on
> semantic correctness guarantees such as snapshot–delta validation or replay.
> Systems requiring such guarantees must use the Wirekrak Market or Core APIs.

### Gateway Layer
- Translates Wirekrak Lite `BookLevel` updates into matching-engine orders
- Normalizes floating-point prices and quantities into integer ticks
- Feeds orders into the Flashstrike engine deterministically
- Collects execution events and basic runtime metrics

### Flashstrike Matching Engine
- Processes normalized limit orders
- Maintains deterministic order book state
- Emits trade events via lock-free rings
- Provides optional telemetry and instrumentation

---

## Gateway Design

The `Gateway` class acts as the boundary between market data ingestion and execution logic:

- Receives `lite::BookLevel` updates via callbacks
- Ignores zero-quantity levels explicitly
- Generates limit orders per price level
- Enforces tick-size normalization prior to engine ingestion
- Avoids floating-point math inside the matching engine
- Periodically drains and reports trade events

This mirrors real-world exchange ingestion pipelines where market data, normalization,
and execution engines remain loosely coupled.

---

## Runtime Behavior

- Runs indefinitely until interrupted (Ctrl+C)
- Uses explicit polling (`client.poll()`)
- Supports optional snapshot requests at subscription time
- Logs progress and telemetry at configurable intervals
- Cleanly unsubscribes and drains events on shutdown

All concurrency and scheduling decisions are explicit and visible to the user.

---

## Purpose

This example exists to demonstrate:

- How **Wirekrak Lite** can feed external execution engines
- Clean separation between transport, ingestion, and execution layers
- Deterministic normalization suitable for low-latency systems
- Practical, production-adjacent integration patterns

It is **not intended as a trading strategy**, nor as a correctness-guaranteed market
data pipeline.

---

## How to Build

This example is gated behind an explicit CMake option and is **not built by default**.

Enable integration builds explicitly:

```
cmake --preset ninja-integrations
cmake --build --preset integrations
```

---

## Notes

- Flashstrike is integrated as a header-only dependency
- Instrumentation and telemetry are optional
- All logic is driven via explicit polling
- No async runtimes or background workers are used
- This example intentionally avoids Market-layer semantics

---

⬅️ [Back to README](../../README.md#flashstrike)
