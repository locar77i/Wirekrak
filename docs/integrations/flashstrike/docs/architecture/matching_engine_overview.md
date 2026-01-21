# Flashstrike Matching Engine — Top-Level Architecture Overview

## 1. Introduction
Flashstrike is a **high‑performance, low‑latency matching engine** written in C++ and designed with the same architectural principles used in leading exchanges and HFT systems.  
Its design emphasizes:
- **Deterministic performance**
- **Zero heap usage in the hot path**
- **Intrusive data structures**
- **Cache‑efficient memory layouts**
- **Predictable O(1) operations**
- **Preallocated fixed-size components**

This document provides a **unified overview** of all core subsystems and how they interact.

---

## 2. High-Level Architecture

The engine is built from the following layered subsystems:

```
┌──────────────────────┐
│      Manager         │  ← Entry point (process, modify, cancel)
└──────────┬───────────┘
           │
┌──────────▼───────────┐
│      OrderBook       │  ← Coordinates all price/partition stores
└───────┬───────┬──────┘
        │       │
        │       │
┌───────▼───┐ ┌─▼─────────┐
│ PriceLevel│ │OrderIdMap │  ← O(1) ID → index lookup
│  Store    │ └───────────┘
└───────┬───┘
        │
┌───────▼───────────┐
│    PartitionPool  │  ← Preallocated partitions
└────────┬──────────┘
         │
┌────────▼──────────┐
│     OrderPool     │  ← Preallocated intrusive order slots
└───────────────────┘

+ SPSC Trades Ring (lock-free)
+ Telemetry (Init, Low-level, Manager)
```

---

## 3. The Manager  
**The Matching Engine Manager** is the top-level orchestration component ([see manager.md](./matching_engine/manager.md)).

Responsibilities:
- Validates incoming orders  
- Dispatches to the appropriate side (BID / ASK)  
- Runs matching loops  
- Uses OrderBook for insert/modify/cancel  
- Emits trade events to a **lock-free SPSC ring**  
- Maintains metrics (latency, queue depth, order pool usage)

Key properties:
- Fully inline hot-path  
- No heap allocations  
- Templated to eliminate branching on BID/ASK paths  
- Processes millions of ops/sec  

---

## 4. OrderBook  
The `OrderBook` owns and coordinates all memory and routing structures ([see order_book.md](./matching_engine/order_book.md)).

Responsibilities:
- Allocates orders (OrderPool)
- Maps order IDs to internal indices (OrderIdMap)
- Allocates partitions for price levels (PartitionPool)
- Routes operations to the BID/ASK PriceLevelStore

Key strength:
> All subsystems remain independent, but the OrderBook gives them meaning as a unified market structure.

---

## 5. PriceLevelStore  
The **PriceLevelStore** is the heart of the price-time priority model ([see price_level_store.md](./matching_engine/price_level_store.md)).

It provides:
- O(1) access to the **best price**
- Partition-based storage (fast, fixed-size)
- Zero allocations—uses PartitionPool under the hood
- Intrusive FIFO queue per price level
- Optimized matching loops
- `Flashstrike` optimizations for repricing without full recompute

Key features:
- Bitmap-based active level tracking  
- Global best bid / best ask tracking  
- Per-partition best-price recompute when needed  
- Avoids full-table scans  

This subsystem guarantees that the engine can access:
```
best BID, best ASK
best price level
first resting order at that price
```
in **O(1)**.

---

## 6. Partition & PartitionPool  
A **Partition** holds a fixed-size array of consecutive price levels ([see partitions.md](./matching_engine/partitions.md)).

PartitionPool:
- Preallocates N partitions  
- Provides O(1) allocate / release  
- Each partition initializes:
  - contiguous price levels
  - active bitmaps
  - best-price tracking  
- Recycled when empty (future optimization path)

This design drastically reduces:
- memory fragmentation  
- cache misses  
- pointer chasing  
- unpredictable latency  

---

## 7. OrderPool  
The **OrderPool** is a fixed-size intrusive memory pool ([see order_pool.md](./matching_engine/order_pool.md)):

- Holds every active order  
- O(1) allocate and release  
- Uses intrusive free list (`next_free`)  
- Perfect for price-level FIFO queues  

Each order embeds:
```
prev_idx
next_idx
next_free
id
price
qty
side
...
```

This allows:
- zero heap usage
- stable memory addresses
- extremely low CPU cache miss rate  

---

## 8. OrderIdMap  
A **fixed-size, power-of-two**, linear-probing hash map mapping ([see order_id_map.md](./matching_engine/order_id_map.md)):
```
order_id → internal order index
```

Features:
- multiplicative hashing to scramble sequential IDs  
- tombstone deletion  
- predictable probe lengths under load factor 0.5  
- essential for fast cancels/modify  

Performance:
```
insert:  O(1)
find:    O(1)
remove:  O(1)
```

---

## 9. Trade Event Pipeline (Lock-Free Ring)  
Executed by Manager:

- Single-producer, single-consumer ring  
- Zero locks  
- Wait-free for consumer  
- Real-time safe for producer  

Used for downstream:
- market data  
- auditing  
- PnL tracking  
- clearing  
- gateway broadcasting  

---

## 10. Telemetry  
Flashstrike integrates **granular telemetrics** ([see telemetry.md](./matching_engine/telemetry.md)):

- initialization metrics  
- low-level memory metrics  
- hot-path operation metrics  
- match-loop stats  
- partition/best-price recomputes  
- ring buffer pressure monitoring  

This is critical for profiling and validating deterministic performance.

### 10.1 Snapshotter: Read-Stable Telemetry Replication

Telemetry is extremely hot and continuously updated by the matching engine’s main processing thread.
If multiple external consumers tried to read it directly—dashboards, CLI tools, exporters—this could disturb cache locality and introduce latency spikes.

To avoid interference, Flashstrike provides a generic utility ([see snapshotter.md](../lcr/metrics/runtime/snapshotter.md)):
```
lcr::metrics::runtime::snapshotter<T> — Low-Overhead Metric Replication
```

The snapshotter creates a stable, atomic, read-only view of all telemetry metrics at a configurable interval (default: 1 second).
It works by maintaining two cache-aligned buffers and performing periodic A/B swaps.

#### Why it matters
- External readers must not touch the hot telemetry cache lines.
- Some dashboards may poll metrics hundreds of times per second.
- Exporters (Prometheus, Grafana agents, internal monitors) benefit from coherent snapshots.

#### With the snapshotter:
- Only one thread copies the metrics (the snapshot worker).
- All readers observe a consistent, interference-free snapshot.
- The matching engine’s hot path remains untouched.

#### Benefits
- Zero contention with hot path.
- Coherent multi-field snapshots (atomic visibility).
- Supports multiple concurrent readers safely.
- Extremely low overhead (copy is cache-local, predictable).

---

## 11. Timing & Timestamp Semantics

Accurate, monotonic, and high-resolution timestamps are foundational to the Flashstrike matching engine.
They drive sequencing, ordering, latency measurement, event tracing, and cross-component coordination.

Flashstrike uses its own timing subsystem built around the CPU’s Time Stamp Counter (TSC), rather than relying on high-latency OS clocks, to provide:

- monotonic, nanosecond timestamps,
- deterministic sequencing,
- precise latency measurement,
- stable numeric timing behavior across cores and over time..

The implementation is provided in the ```monotonic_clock``` module ([see monotonic_clock.md](../lcr/system/monotonic_clock.md)), which provides the foundation for all time-dependent logic in the engine..

---

## 12. Deterministic Performance Model

### No dynamic memory
All hot-path structures are preallocated:
- OrderPool  
- PartitionPool  
- IdMap  
- Price levels  
- Trades ring  

### No syscalls  
No I/O on the hot path.

### O(1) for all major operations
Operation | Complexity
---------|-----------
match     | amortized O(1) per matched order
insert    | O(1)
cancel    | O(1)
modify    | O(1)
find      | O(1)
best bid/ask | O(1)

### Highly predictable cache layout
- contiguous arrays  
- intrusive lists  
- no pointer indirection layers  

---

## 13. Summary

The Flashstrike matching engine architecture exhibits:

- **Professional-grade exchange design**
- **Deterministic low-latency behavior**
- **Zero heap allocations**
- **Fixed-size memory pools**
- **Perfect O(1) operations**
- **Modular, testable subsystems**
- **High cache locality & SIMD-friendly layouts**
- **Scalability to millions of orders**

It mirrors the design philosophies used by:
- equities exchanges  
- crypto exchanges  
- prop firm internal engines  
- FPGA routers  
- ultra-low-latency HFT systems  

This document represents the unified architecture of the matching engine and situates every component in context.

---

## 13. Component Documentation

Flashstrike’s matching engine is composed of several focused, deterministic, high-performance subsystems:

| Component | Description |
|----------|-------------|
| [Manager](./matching_engine/manager.md) | Orchestrates validation, matching, order flow, and trade events. |
| [OrderBook](./matching_engine/order_book.md) | Coordinates pools, stores, and ID maps; central market state. |
| [OrderPool](./matching_engine/order_pool.md) | Intrusive preallocated memory pool for ultra-fast order storage. |
| [OrderIDMap](./matching_engine/order_id_map.md) | Power-of-two linear-probing hash map for O(1) id lookup. |
| [PriceLevelStore](./matching_engine/price_level_store.md) | Partitioned price-time priority engine with best-price tracking. |
| [Partitions](./matching_engine/partitions.md) | Partition + PartitionPool architecture for scalable price ranges. |

---

👉 Back to [`Flashstrike — High-Performance Matching Engine Documentation`](../../index.md)
