# Threading Model Rationale: 2 Threads First, 3 Threads Later

This document explains **why Wirekrak currently uses a 2-thread (2T) execution model**,
why this decision is **appropriate and professional for the current stage**, and why a
**3-thread (3T) model is intentionally postponed** until proper benchmarking is completed.

This is a deliberate engineering decision, not a limitation.

---

## Current Execution Model (2 Threads)

Wirekrak operates with **two clearly separated threads**:

```
┌────────────────────────────┐
│ Transport / Network Thread │
│ (WinHTTP + TLS callbacks)  │
└──────────────▲─────────────┘
               │ WebSocket frames
               │
┌──────────────┴─────────────┐
│ Client / Processing Thread │
│                             │
│ • stream::Client::poll()    │
│ • JSON parsing (simdjson)   │
│ • protocol routing          │
│ • ring-buffer publishing    │
│ • user callbacks            │
└────────────────────────────┘
```

### Responsibilities

**Thread 1 – Transport**
- OS networking
- TLS (SChannel)
- WebSocket framing
- Minimal logic, OS-driven

**Thread 2 – Client**
- Message parsing
- Routing and dispatch
- Liveness & reconnection logic
- User-facing API

---

## Why the 2-Thread Model Is the Right Choice Now

### 1. Deterministic and Lower Latency

- Messages are parsed immediately after reception
- No additional queue hop
- No extra context switch
- Better p99 / p999 latency

For market data systems, **latency determinism matters more than raw throughput**.

---

### 2. Reduced Complexity and Risk

- Fewer synchronization points
- Clear ownership of state
- Easier shutdown semantics
- Fewer race conditions

This is especially important for a hackathon, where **clarity and correctness**
are judged as much as performance.

---

### 3. Lock-Free Design Already Scales

Wirekrak already uses:

- Single-producer / single-consumer lock-free rings
- No heap allocations in hot paths
- Cache-friendly data structures

This means:
- Parsing is not currently a bottleneck
- Adding threads without pressure provides no benefit

---

### 4. Parsing Is Not Proven to Be the Bottleneck

In real Kraken feeds:
- Network latency dominates
- TLS dominates
- Kernel scheduling dominates

Until benchmarks prove otherwise:
> Moving the parser to its own thread is speculative optimization.

---

## Why a 3-Thread Model Is Not Implemented Yet

### The Proposed 3T Model

```
┌──────────────┐
│ Transport    │
└─────▲────────┘
      │
┌─────┴────────┐
│ Parser Thread│
└─────▲────────┘
      │
┌─────┴────────┐
│ Client / User│
└──────────────┘
```

### Downsides Without Evidence

- Extra buffering and queues
- Increased latency
- Additional memory fences
- More complex lifecycle management
- Harder debugging and testing

Without benchmarks, this **adds risk without measurable gain**.

---

## Professional Engineering Principle Applied

> **Measure first. Optimize later.**

Wirekrak follows this principle strictly:

- Architecture already allows adding a parser thread
- No refactor of protocol or transport layers will be needed
- Only wiring and scheduling would change

---

## Roadmap (Post-Hackathon)

The 3-thread model will be evaluated once:

- Throughput benchmarks exist
- Latency percentiles are measured
- Parser CPU utilization is proven to saturate a core

Only then will:
- A dedicated parser thread be justified
- Backpressure policies be tuned
- Cache and NUMA behavior be analyzed

---

## Summary

- The 2-thread model is **deliberate and professional**
- It prioritizes correctness, clarity, and latency
- The system is already architected for future scaling
- The 3-thread model is postponed until data justifies it

---

⬅️ [Back to README](../../README.md#threading-model)
