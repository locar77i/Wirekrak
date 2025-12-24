# `PartitionPlan` Struct Documentation

This document provides a full specification of the `flashstrike::matching_engine::conf::PartitionPlan` structure.  
`PartitionPlan` defines the **pure integer discretization layout** for price levels inside the Flashstrike matching engine.

---

## Overview

`PartitionPlan` is an **internal, engine-focused** representation that describes:

- how the price range is split into discrete integer ticks  
- how those ticks are divided into partitions  
- how many ticks and partitions exist  
- the power‑of‑two layout used for memory efficiency  

A `PartitionPlan`:

- **does not know about human-readable units** (USD, BTC, decimals)
- **is always generated from an `Instrument`**
- is used for:
  - mapping prices → partition indices  
  - tick arithmetic  
  - preallocating order book storage  
  - CPU‑efficient partition lookups using bit operations  

---

## Purpose in the Matching Engine

The matching engine uses `PartitionPlan` to:

- compute partition boundaries
- efficiently index price levels
- operate over cache‑friendly blocks of price ticks
- ensure tick counts and partition sizes are round powers of two for:
  - fast masking  
  - shift‑based division  
  - predictable memory alignment  

---

## Field Summary (Private Internal Layout)

Although internal, the conceptual fields are:

| Field | Type | Meaning |
|-------|------|---------|
| `partition_bits_` | `uint32_t` | log2 of ticks per partition (shift amount) |
| `num_partitions_` | `uint32_t` | total number of partitions |
| `partition_size_` | `uint64_t` | number of ticks in a partition (must be power of two) |
| `num_ticks_` | `uint64_t` | total number of discrete ticks across price range |

These are derived values computed from both the market’s `Instrument` and the user‑requested number of partitions.

---

## Accessors

All accessors are `constexpr` and `noexcept`:

```cpp
uint32_t partition_bits() const noexcept;
uint32_t num_partitions() const noexcept;
uint64_t partition_size() const noexcept;
uint64_t num_ticks() const noexcept;
```

---

## Core Method — `compute()`

The main operation of a `PartitionPlan` is:

```cpp
NormalizedInstrument compute(const Instrument& instrument,
                             std::uint32_t target_num_partitions) noexcept;
```

### What `compute()` Does

1. **Validates instrument tick and max price inputs**
2. **Normalizes price tick size**
   - Converts `price_tick_units` → integer tick size
3. **Scales maximum price into scaled integer form**
4. **Computes number of discrete ticks**
   ```
   num_ticks = price_max_scaled / price_tick_size
   ```
5. **Rounds both `num_ticks` and `target_num_partitions` up to next power of two**
6. **Computes:**
   - number of partitions  
   - ticks per partition  
   - `partition_bits = log2(partition_size)`  
7. **Returns a fully normalized `NormalizedInstrument`**
   - using `instrument.normalize(num_ticks)`  

This connects the semantic `Instrument` object with the mechanical `PartitionPlan`.

---

## Power-of‑Two Rounding

The engine uses power‑of‑two partition and tick counts because they enable:

- constant‑time bit masking  
- cheap log2 extraction  
- array indexing without division  

This is critical for matching‑engine performance.

---

## Example Usage

```cpp
Instrument inst = ...;

PartitionPlan plan;
NormalizedInstrument ni = plan.compute(inst, 64);

std::cout << plan.to_string() << std::endl;
```

A typical configuration might produce:

- 1,048,576 total ticks  
- 64 partitions  
- 16,384 ticks per partition  
- `partition_bits = 14`  

---

## Structural Safety

The struct is required to be:

```cpp
static_assert(std::is_trivially_copyable_v<PartitionPlan>);
static_assert(std::is_standard_layout_v<PartitionPlan>);
```

This ensures:

- no vtable  
- no dynamic memory  
- safe for memcpy  
- predictable binary layout  

---

## Summary

`PartitionPlan` is the **mechanical backbone** of the matching engine’s price-domain representation.  
It defines how price ticks are discretized, aligned, and partitioned to guarantee:

- deterministic integer behavior  
- memory‑efficient lookup  
- high‑performance order‑book operations  

It works hand‑in‑hand with:

- `Instrument` (human-friendly definition)  
- `NormalizedInstrument` (scaled integer format)  

All three form the full representation of a tradable market inside Flashstrike.

---
 
## Related components

[`matching_engine::Manager`](../manager.md)
[`matching_engine::OrderBook`](../order_book.md)
[`matching_engine::OrderIdMap`](../order_id_map.md)
[`matching_engine::OrderPool`](../order_pool.md)
[`matching_engine::PartitionPool`](../partitions.md)
[`matching_engine::PriceLevelStore`](../price_level_store.md)
[`matching_engine::Telemetry`](../telemetry.md)

[`matching_engine::conf::Instrument`](./instrument.md)
[`matching_engine::conf::NormalizedInstrument`](./normalized_instrument.md)

---

👉 Back to [`Manager - Matching Engine Orchestrator`](../manager.md)
