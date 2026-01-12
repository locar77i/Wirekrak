# Experimental Kraken → FlashStrike Gateway

## Overview

This experimental example demonstrates how **Wirekrak** can be used as a real-time
market data ingestion layer feeding a **production-grade matching engine**.

The example connects to the **Kraken WebSocket API v2**, subscribes to level-2
order book data, normalizes prices and quantities, and injects them directly
into the FlashStrike matching engine for live processing.

This integration is intentionally built as an **opt-in experiment** and does not
affect Wirekrak’s core library or default builds.

---

## Architecture

The example is structured around a clear separation of responsibilities:

### Wirekrak Client
- Manages WebSocket connection lifecycle
- Handles reconnection, liveness, and subscription replay
- Delivers typed Kraken book messages

### Gateway Layer
- Translates Kraken book updates into matching-engine orders
- Normalizes floating-point prices and quantities into integer ticks
- Feeds orders into the FlashStrike engine deterministically
- Collects execution events and basic metrics

### FlashStrike Matching Engine
- Processes normalized limit orders
- Maintains deterministic order book state
- Emits trade events via lock-free rings
- Provides optional telemetry and instrumentation

---

## Gateway Design

The `Gateway` class acts as the boundary between market data and execution logic:

- Receives `book::Book` updates
- Iterates bid and ask levels independently
- Generates limit orders per price level
- Enforces tick-size normalization
- Avoids floating-point math inside the engine
- Periodically drains and reports trade events

This mirrors real-world exchange ingestion pipelines where market data and
matching engines remain loosely coupled.

---

## Runtime Behavior

- Runs indefinitely until interrupted
- Uses explicit polling (no background threads)
- Supports optional snapshot requests
- Logs progress and telemetry at configurable intervals
- Cleanly unsubscribes and drains events on shutdown

---

## Purpose

This example exists to demonstrate:

- How Wirekrak can feed external execution engines
- Clean separation between transport, protocol, and execution layers
- Deterministic normalization suitable for low-latency systems
- Practical, production-adjacent integration patterns

It is **not intended as a trading strategy**, but as a systems integration showcase.

---

## How to Build

This example is gated behind an explicit CMake option and is **not built by default**.

Enable experimental builds explicitly:

```
cmake --preset ninja-experimental
cmake --build --preset experimental
```

---

## Notes

- FlashStrike is integrated as a header-only dependency
- Instrumentation and telemetry are optional
- All logic is driven via explicit polling
- No async runtimes or background workers are used

---

⬅️ [Back to README](../../README.md#flashstrike-examples)
