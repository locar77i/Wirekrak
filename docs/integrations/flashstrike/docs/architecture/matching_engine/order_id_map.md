# OrderIdMap — Architecture Overview

## Purpose
`OrderIdMap` provides a **high‑performance, fixed‑size, open‑addressing hash map** used to resolve:
```
OrderId → OrderPool index
```

This enables the matching engine to perform:
- **O(1) cancels**
- **O(1) modifies**
- **O(1) direct access to active orders**

It is a mandatory subsystem for exchange‑grade matching engines, where external clients reference orders by ID.

---

## High‑Level Design

`OrderIdMap` is a **preallocated, power‑of‑two hash table** with:
- no dynamic resize
- no heap usage in the hot‑path
- linear probing for collision resolution
- multiplicative hashing for sequential IDs
- tombstones for deletions

This design yields:
- predictable latency
- cache‑friendly sequential probing
- consistent performance under churn

---

## Memory Layout

Each entry in the table consists of:

```
struct Entry {
    OrderId  key;
    OrderIdx val;
};
```

Two sentinel values are used:
- `EMPTY_KEY = 0`
- `TOMBSTONE_KEY = max(OrderId)`

The table is a contiguous vector `<Entry>`.

---

## Capacity & Load Factor

Initialization:
- User requests `capacity`
- Internally multiplied by `2.0` → **target load factor = 0.5**
- Rounded **up** to next power of two  
- Mask generated: `mask = capacity - 1`

This ensures:
```
index = hash & mask
```
is extremely fast.

---

## Hash Function

A Knuth multiplicative hash scrambles sequentially increasing order IDs:

```
h = id * 2654435761
```

This reduces clustering for workloads where order IDs are monotonic (typical in exchanges).

---

## Probing Strategy: Linear Probing

For insert/find/remove:

```
idx = (hash + i) & mask   // i increments on collision
```

Advantages:
- Sequential memory access (cache‑efficient)
- Very low branch misprediction rate
- Optimal when load factor ≤ 0.5

---

## Insertion

Insertion succeeds if:
- slot is EMPTY, or
- slot is TOMBSTONE

On success:
- entry is written
- size_ increments

Worst‑case probe chain:
- bounded by table capacity (but typically ≤ 4 at LF=0.5)

---

## Deletion (Tombstoning)

Removing a key:
- marks entry with `TOMBSTONE_KEY`
- keeps probe chain intact
- avoids expensive backward shifts

This preserves correctness of future lookups.

---

## Lookup

Lookup stops when:
```
EMPTY_KEY encountered → key not present
```

Otherwise:
- key matches → return pool index  
- keep probing if collision occurred

---

## 1Performance Characteristics

### Time Complexity
Operation | Complexity | Notes
---------|------------|------
insert   | O(1) avg | bounded probe chain
find     | O(1) avg | extremely fast under LF 0.5
remove   | O(1) avg | tombstones preserve chain
clear    | O(n) | linear reset

### No Resizing
Critical for real‑time engines:
- predictable timing
- no rehash pauses
- no memory churn

---

## Telemetry & Instrumentation

Integrated metric hooks:
- probe length
- success/failure
- table capacity
- memory footprint

These allow:
- profiling real‑market workloads
- detecting hot‑spots
- fine‑tuning load factor

---

## Why This Matters

The design mirrors hash maps found in:
- high‑frequency trading engines  
- exchange‑grade order books  
- low‑latency gateways  

It is:
- predictable  
- safe  
- scalable  
- cache‑efficient  
- perfect for cancel/modify hot‑paths  

---

## Summary

`OrderIdMap` provides:

- **Constant‑time ID lookup**
- **Zero dynamic memory**
- **Open addressing with linear probing**
- **Tombstone‑based removal**
- **Ideal performance at LF=0.5**
- **Exchange‑grade determinism**
- **Tight CPU‑cache locality**

This makes it a foundational piece of the Flashstrike matching engine memory subsystem.

---
 
## Related components

[`matching_engine::Manager`](./manager.md)
[`matching_engine::OrderBook`](./order_book.md)
[`matching_engine::OrderPool`](./order_pool.md)
[`matching_engine::PartitionPool`](./partitions.md)
[`matching_engine::PriceLevelStore`](./price_level_store.md)
[`matching_engine::Telemetry`](./telemetry.md)

[`matching_engine::conf::Instrument`](./conf/instrument.md)
[`matching_engine::conf::NormalizedInstrument`](./conf/normalized_instrument.md)
[`matching_engine::conf::PartitionPlan`](./conf/partition_plan.md)

---

👉 Back to [`Flashstrike Matching Engine — Overview`](../matching_engine_overview.md)
