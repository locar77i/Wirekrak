# Flashstrike PriceLevelStore — Architecture Overview

## Purpose
The `PriceLevelStore` is the core low‑level data structure that powers the Flashstrike matching engine’s **ultra‑low‑latency, O(1) price access**.  
It organizes all price levels using a **partitioned, bitmap‑indexed layout** with strict price‑time ordering and predictable memory access patterns.

This component is designed to mirror the performance characteristics of modern exchange engines.

---

## High-Level Design

Each `PriceLevelStore` manages one side of the book:

```
PriceLevelStore<BID>
PriceLevelStore<ASK>
```

It provides:
- O(1) best price lookup  
- O(1) insert/remove at any price  
- branch‑free price comparisons (via template specialization)  
- FIFO ordering inside each price level  
- minimal cache misses during matching  

The store works by partitioning the price space:

```
price → partition_id → price_level_offset
```

---

## Partitioned Price Space

Prices are divided into **fixed‑size partitions**, determined by:

- `num_partitions`
- `partition_bits`
- `partition_mask`

This allows:

### ✔ constant‑time mapping  
```
partition_id  = price >> partition_bits
offset_in_partition = price & partition_mask
```

### ✔ extremely compact memory layout  
Each partition contains:
- a contiguous array of `PriceLevel`
- its own "best price"
- active level bits

### ✔ predictable L1/L2 behavior  
All hot data for a matching cycle stays within a small, cache‑friendly region.

---

## Active Partition Bitmap

The store maintains a bitmap:

```
active_bitmap_[]
```

Each bit represents an active partition containing at least one active price level.

To find the next active partition, it uses:

```
__builtin_ctzll(mask)
```

This enables:

### ✔ branch‑free scanning  
### ✔ extremely fast best‑price discovery  
### ✔ O(1) recomputation of global best price

This technique is used in high‑performance matchers where tail latency matters.

---

## Global Best‑Price Tracking

The store caches the global best price:

- `best_price_`
- `has_best_ = true|false`

### Fast‑path: incremental update  
On insertion or repricing, the best price updates if the new price is better.

### Slow‑path: bitmap recomputation  
Triggered only when:
- best price level becomes empty  
- the best partition changes  

Uses the active bitmap for fast scanning.

This approach reduces tail latency by avoiding full recomputation on every mutation.

---

## Flashstrike Optimized Repricing

A key innovation in this engine is:

### **`try_to_reprice_order_by_flashstrike_()`**

This algorithm determines whether a repricing operation can be performed **without** triggering:

- partition‑wide best recomputation  
- global best recomputation  
- unlinking/relinking across partitions

### Fast‑Path Conditions
1. Reprice within the same partition  
2. Order is not the best in that partition  
3. Or order improves the global best price  
4. Or moving to a different partition where old partition best does not change

### Benefit:
> Repricing becomes almost always O(1) and branch‑free.

This dramatically reduces tail spikes during bursts of repricing traffic.

---

## PriceLevel Internals

Each price level contains:
- head/tail order indices  
- total quantity  
- active flag  
- strict FIFO linked list of orders (using index‑based prev/next pointers)

### Properties:
- no heap allocation  
- no iterator invalidation  
- stable ordering  
- O(1) operations for insert/remove  
- perfect price–time priority enforcement

---

## Remove and Insert Logic

Insert:
```
Partition* p = get_or_create_partition(partition_id(price))
link_order(order_idx, order, p)
update partition best
update global best
```

Remove:
```
unlink_order(order_idx, order, p)
recompute partition best if needed
recompute global best if needed
```

### Guarantees:
- fully deterministic
- constant‑time
- no syscalls
- no unpredictable branches

---

## Complexity Summary

| Operation                | Complexity | Notes |
|--------------------------|------------|-------|
| Insert                   | O(1) | partition + level insert |
| Remove                   | O(1) | unlink + local best update |
| Resize                   | O(1) | quantity mutation only |
| Reprice (fast‑path)      | O(1) | Flashstrike optimization |
| Reprice (slow‑path)      | O(log N) worst case | bitmap scan |
| Best‑price lookup        | O(1) | cached |

---

## Performance Characteristics

### ✔ Zero dynamic allocation  
Partitions and levels are preallocated in a pool.

### ✔ Lock‑free design  
This structure is used exclusively by the matching thread.

### ✔ Cache‑optimized  
Partitioned layout ensures hot data stays in small working sets.

### ✔ Deterministic matching  
Everything is index‑based and predictable.

### ✔ Tail‑latency aware  
Flashstrike optimization + bitmap recompute minimize worst‑case spikes.

---

## Why This Architecture Matters

This design mirrors the architectural principles of real exchange engines:

- NASDAQ INET  
- Kraken X‑Engine  
- Coinbase Pro LOB  
- Jump/Jane Street proprietary matchers  
- FPGA‑accelerated price‑level engines  

It delivers:
- extremely predictable latency  
- efficient burst performance  
- safe, index‑based memory handling  
- full price–time priority compliance  
- horizontal scalability (per partition / per instrument)  

---

## Summary

The Flashstrike `PriceLevelStore` is a high‑performance, feature‑rich subsystem engineered for exchange‑grade workloads.  
Its unique characteristics:

- partition‑aware price mapping  
- bitmap‑accelerated best‑price discovery  
- optimized repricing (“Flashstrike”)  
- strict FIFO ordering  
- zero‑allocation, lock‑free operations  

make it one of the most advanced components of the Flashstrike matching engine.

---
 
## Related components

[`matching_engine::Manager`](./manager.md)
[`matching_engine::OrderBook`](./order_book.md)
[`matching_engine::OrderIdMap`](./order_id_map.md)
[`matching_engine::OrderPool`](./order_pool.md)
[`matching_engine::PartitionPool`](./partitions.md)
[`matching_engine::Telemetry`](./telemetry.md)

[`matching_engine::conf::Instrument`](./conf/instrument.md)
[`matching_engine::conf::NormalizedInstrument`](./conf/normalized_instrument.md)
[`matching_engine::conf::PartitionPlan`](./conf/partition_plan.md)

---

👉 Back to [`Flashstrike Matching Engine — Overview`](../matching_engine_overview.md)
