# Wirekrak Examples

This directory contains progressively structured examples showing how to use
Wirekrak at different abstraction levels.

The examples are intentionally layered. Start at the level that matches your
needs and experience.

---

## Quick Start (2–3 minutes)

If you just want to stream Kraken market data with minimal setup:

**Start here:** ➡️ [Wirekrak Lite Examples](./lite/README.md)   `examples/lite/`


These examples demonstrate the `wirekrak::lite` client:
- opinionated defaults
- minimal configuration
- fast time-to-data
- suitable for most consumers

---

## Conceptual Map

Wirekrak supports **two distinct usage paths** or primary layers, depending on what you want
to build and how deep you want to go:

```
┌────────────────┐
│  `lite client` │  ← simple, opinionated, low ceremony
└───────┬────────┘
        │
        ▼
┌────────────────┐
│ `core client`  │  ← full control, visibility, replay, metrics
│   ├─ contracts │
│   └─ transport │
└────────────────┘
```

---

## `lite/` — High-Level Streaming Client

**Who this is for**
- You want live market data quickly
- You do not need protocol-level control
- You prefer a stable, narrow API

**What you’ll learn**
- Connecting to Kraken’s WS API
- Subscribing to market data
- Consuming normalized events
- Handling lifecycle and shutdown

➡️ [Wirekrak Lite Examples](./lite/README.md)

---

## `core/` — Low-Level Control and Visibility

**Who this is for**
- You need deterministic behavior
- You care about replay, metrics, and internals
- You want to integrate Wirekrak into a larger system

➡️ [Wirekrak Core Examples](./core/README.md)

---

### Expectations and Invariants — `core/contracts/`

**Who this is for**
- You want to understand Wirekrak’s correctness model
- You need guarantees around ordering, visibility, or delivery

---

### Connection and WebSocket Control — `core/transport/`

**Who this is for**
- You need explicit control over connection behavior
- You are integrating Wirekrak into an existing networking stack
- You care about failure modes, reconnect logic, and state transitions

These examples intentionally avoid hiding complexity.

---

## Common Utilities

`examples/common/` contains shared CLI helpers and utilities used across all
examples.

---

## How to Read the Examples

Examples are designed to be:
- runnable
- minimal but realistic
- composable

---

## Not Sure Where to Start?

- Want data fast? → `lite/`
- Want control and visibility? → `core/`
- Want guarantees? → `core/contracts/`
