# FlashStrike OrderPool — Architecture Overview

## Purpose
The `OrderPool` is the FlashStrike matching engine’s **high-performance, preallocated memory subsystem**
responsible for storing all active orders. It is designed for:
- **Zero heap usage in the hot path**
- **Deterministic O(1) allocation and release**
- **Stable intrusive-list operations**
- **Predictable latency under load**

This architecture is standard in real matching engines, HFT systems, FPGA engines, and exchange backends.

---

## High-Level Design

The OrderPool is a **fixed-size contiguous array** of `Order` objects:

```
pool_[0], pool_[1], ..., pool_[max_orders - 1]
```

Memory is reserved at engine startup, ensuring:
- No dynamic memory allocation
- No fragmentation
- Clear CPU cache patterns
- Deterministic worst-case latency

Each order slot can be in two states:
- **free**
- **allocated**

State transitions occur via an intrusive free list.

---

## Intrusive Free List

Each `Order` contains:
```
OrderIdx next_free;
```

This builds an internal stack-based free list:

```
 free_head_ → [order A] → [order B] → [order C]...
```

### Properties:
- **O(1) allocate**: pop from free list
- **O(1) release**: push back to free list
- **No additional memory overhead**
- **No allocations/deallocations from the heap**

This is the ideal strategy for HFT workloads.

---

## Order Structure Layout

Each `Order` stores:
- `prev_idx`, `next_idx` (intrusive list for FIFO price levels)
- `next_free` (intrusive freelist pointer)
- `id`
- `type`, `tif`
- `price`
- `qty`, `filled`
- `side`

This means the order pool integrates seamlessly with:
- PriceLevel FIFO queues
- PriceLevelStore repricing/removal
- Matching and cancel logic

No wrapper objects. No pointers. No allocation overhead.

---

## Key Operations

### Allocate (`allocate()`)
```
OrderIdx idx = free_head_;
free_head_ = pool_[idx].next_free;
pool_[idx].next_free = INVALID_INDEX;
```

Fast, cache-friendly, unconditional.

### Release (`release(idx)`)
```
pool_[idx].next_free = free_head_;
free_head_ = idx;
```

Direct push onto the free list.

### Access (`get(idx)`)
Direct array indexing — no hashing, no tree traversal.

---

## Deterministic Performance

### ✔ No malloc/free  
This eliminates:
- memory fragmentation
- allocator bottlenecks
- unpredictable latency spikes

### ✔ Constant-time complexity  
All operations are O(1), with no variation based on load.

### ✔ Perfect for 1M+ orders  
Large pools behave identically to small ones due to stable complexity.

---

## Debug Mode (Optional)

When `DEBUG` is enabled:
- Double-allocate detection  
- Double-free detection  
- Accessing freed order checks  

These checks:
- catch logic bugs early
- impose **zero overhead in production** when disabled

This mirrors the design philosophy of professional engines.

---

## Metrics & Telemetry

OrderPool integrates with the metrics layer:

- allocation count  
- release count  
- memory footprint reporting  
- pool usage (free vs used slots)

This supports:
- monitoring during stress tests
- capacity planning
- profiling

---

## Memory Footprint

The pool reports:
- static bytes (OrderPool struct)
- dynamic bytes (order array, allocation flags)

This is useful for:
- sizing pools per instrument  
- multi-instrument deployments  
- exchange-grade capacity dashboards  

---

## Why This Architecture Matters

OrderPool’s design matches the memory strategies used in:
- major crypto exchanges  
- equities exchanges (NASDAQ, NYSE technology stack)  
- prop trading engines  
- FPGA-based order books  

It is:
- **simple**
- **fast**
- **predictable**
- **production-proven**

A scalable matching engine must have an allocation subsystem like this.

---

## Summary

FlashStrike’s `OrderPool` provides:
- O(1) deterministic order allocation
- zero heap usage in the hot-path
- cache-friendly contiguous storage
- safe intrusive memory management
- perfect compatibility with the FIFO PriceLevel architecture

This subsystem is foundational for achieving high throughput and low latency across the entire engine.

---
 
## Related components

[`matching_engine::Manager`](./manager.md)
[`matching_engine::OrderBook`](./order_book.md)
[`matching_engine::OrderIdMap`](./order_id_map.md)
[`matching_engine::PartitionPool`](./partitions.md)
[`matching_engine::PriceLevelStore`](./price_level_store.md)
[`matching_engine::Telemetry`](./telemetry.md)

[`matching_engine::conf::Instrument`](./conf/instrument.md)
[`matching_engine::conf::NormalizedInstrument`](./conf/normalized_instrument.md)
[`matching_engine::conf::PartitionPlan`](./conf/partition_plan.md)

---

👉 Back to [`FlashStrike Matching Engine — Overview`](../matching_engine_overview.md)
