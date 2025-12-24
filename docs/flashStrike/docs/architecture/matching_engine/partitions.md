# FlashStrike Partition Architecture — Design Overview

## Purpose
The **Partition** and **PartitionPool** subsystems form the foundation of FlashStrike’s
ultra-low-latency matching engine.  
They provide **deterministic memory layout**, **constant-time price-to-level mapping**,  
and **cache-optimized traversal**, enabling the PriceLevelStore to operate with
exchange-grade performance.

This document summarizes the architecture and design philosophy of:
- `Partition`
- `PriceLevel`
- `PartitionPool`

---

## Architectural Role

FlashStrike decomposes the price space into fixed-size **partitions**:

```
price → partition_id → price_level_offset
```

This ensures that:
- Lookup operations are O(1)
- Price levels are stored contiguously
- Hot data stays localized in L1/L2 cache
- Matching and repricing avoid random memory access

The partition system provides the structural backbone for:
- PriceLevelStore<BID>
- PriceLevelStore<ASK>
- FlashStrike repricing optimization
- Best-price tracking

---

## PriceLevel — Intrusive FIFO Queue per Price

A `PriceLevel` groups all orders resting at the same price.

It stores:
- head/tail order indices (intrusive linked list)
- total aggregated quantity
- active flag
- price value

### Key Properties
- **Zero heap allocation**: all order nodes are in OrderPool
- **Strict FIFO ordering**: ensures price–time priority
- **O(1) insert/remove**
- **Constant-time quantity updates**

Each `PriceLevel` is extremely compact and optimized for CPU cache locality.

---

## Partition — Fixed-Range Container of PriceLevels

A `Partition` contains a fixed number of `PriceLevel` objects, representing
a contiguous block of prices:

```
partition[k] = prices [k * size, (k+1)*size - 1]
```

### Each Partition contains:
- A contiguous array: `std::vector<PriceLevel>`
- A bitmap marking active price levels
- Partition-local best price
- `min_price()` / `max_price()`

### Key Responsibilities
- Tracks whether any level within the partition is active
- Maintains **best price inside the partition** (BID: max, ASK: min)
- Performs **fast local recomputation** of best price when needed
- Supplies active levels for matching operations

### Why This Matters
The matching engine rarely needs to scan the entire price range —
it only scans within the partitions currently referenced by the PriceLevelStore.

This drastically reduces tail latency.

---

## Partition Active-Bitmap

Each Partition tracks which offset-levels are active:

```
bitmap[word] & (1 << bit)
```

### Benefits
- O(1) detection of nonempty partitions
- Optimized active-level scanning
- Limited branching
- Enables vectorizable operations

Combined with the PriceLevelStore’s global active partition bitmap, this forms a
multi-layered indexing system optimized for high-frequency matching.

---

## PartitionPool — Preallocated Memory Manager

The `PartitionPool` is a lightweight allocator for Partitions.

It:
- Preallocates all partitions at startup
- Maintains a stack-based free list
- Returns initialized partitions on demand
- Supports recycling (if enabled)

### Reasons for Separation
- Simplifies memory ownership: the pool only manages slots, not semantics
- Ensures deterministic memory layout
- Makes the matching engine horizontally scalable
- Avoids embedding market semantics into allocation logic

### Pool Properties
- O(1) allocate
- O(1) release
- No heap activity in the hot path
- All partitions reside in a single contiguous array

---

## Lifecycle

### Allocation:
```
Partition* p = partition_pool.allocate(partition_id);
p.initialize_for_partid(partition_id);
```

### Use:
- PriceLevelStore links/unlinks orders
- Partition updates local best prices
- Bitmap tracks active levels

### Release:
```
partition_pool.release(p);
```

(Current engine keeps partitions persistent; release may be used in future multi-asset tiers.)

---

## Complexity Summary

| Operation                | Complexity | Notes |
|--------------------------|------------|-------|
| Partition allocation     | O(1) | Filestore-based, stack pop |
| Partition release        | O(1) | Stack push |
| Price to level mapping   | O(1) | Bitmask + shift |
| Level activation         | O(1) | Single bit set |
| Best-price recomputation | O(N_partitions) worst case | Scans bitmap across partitions |

---

## Why This Architecture Matters

The FlashStrike partition architecture mirrors the structure used in
modern exchange engines and FPGA-backed order books:

- NASDAQ INET price buckets  
- Kraken X-Engine partitioned price queues  
- FPGA / hardware accelerated partitioned LOBs  
- GPU-optimized limit order books  

The design delivers:
- **Zero-allocation hot path**
- **Cache-friendly memory layout**
- **Deterministic latency**
- **Horizontal scalability**
- **Stable matching under burst load**

This architecture allows FlashStrike to behave like a production-grade exchange engine.

---

## Summary

FlashStrike’s partition-based architecture enables:
- O(1) price lookup
- Perfect locality of reference
- Extremely fast repricing and matching
- Minimal tail latency
- High-throughput processing consistent with exchange standards

Together with OrderPool, OrderIdMap, and PriceLevelStore,
the partition subsystem forms a critical piece of the engine's performance profile.

---
 
## Related components

[`matching_engine::Manager`](./manager.md)
[`matching_engine::OrderBook`](./order_book.md)
[`matching_engine::OrderIdMap`](./order_id_map.md)
[`matching_engine::OrderPool`](./order_pool.md)
[`matching_engine::PriceLevelStore`](./price_level_store.md)
[`matching_engine::Telemetry`](./telemetry.md)

[`matching_engine::conf::Instrument`](./conf/instrument.md)
[`matching_engine::conf::NormalizedInstrument`](./conf/normalized_instrument.md)
[`matching_engine::conf::PartitionPlan`](./conf/partition_plan.md)

---

👉 Back to [`FlashStrike Matching Engine — Overview`](../matching_engine_overview.md)
