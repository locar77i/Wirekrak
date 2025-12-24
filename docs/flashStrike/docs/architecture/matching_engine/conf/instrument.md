# `Instrument` Struct Documentation

This document describes the `flashstrike::matching_engine::conf::Instrument` structure, a compact representation of a tradable asset pair used inside the Flashstrike matching engine.

## Overview

`Instrument` models a market such as **BTC/USD**, providing:
- Human-readable tick/precision information
- Bounds for prices and quantities
- Minimum order and notional requirements
- A normalization method that converts floating-point inputs into fully integer-scaled values

It maps loosely to Kraken’s “Tradable Asset Pair” schema but includes only fields needed for the matching engine.

## Field Summary

### Symbol Identifiers
| Field | Type | Description |
|-------|------|-------------|
| `base_symbol[5]` | `char[]` | Base currency (e.g., "BTC") |
| `quote_symbol[5]` | `char[]` | Quote currency (e.g., "USD") |
| `name[10]` | `char[]` | Market symbol (e.g., "BTC/USD") |

### Tick & Precision Settings
| Field | Type | Description |
|-------|------|-------------|
| `price_tick_units` | `double` | Minimum price increment in quote units |
| `qty_tick_units` | `double` | Minimum quantity increment in base units |
| `price_decimals` | `uint8_t` | Decimal precision for price |
| `qty_decimals` | `uint8_t` | Decimal precision for quantity |

### Bounds
| Field | Type | Description |
|-------|------|-------------|
| `price_max_units` | `double` | Maximum allowed price |
| `qty_max_units` | `double` | Maximum allowed quantity |
| `min_qty_units` | `double` | Minimum base quantity |
| `min_notional_units` | `double` | Minimum notional (quote) value |

### Market Metadata
| Field | Type | Description |
|-------|------|-------------|
| `status[16]` | `char[]` | Market status (e.g., "online") |

## Normalization Method

```cpp
NormalizedInstrument normalize(std::uint64_t num_ticks) const noexcept;
```

This converts human-friendly values into deterministic, integer‑scaled internal units.

### Steps Performed
1. Price tick normalization  
2. Quantity tick normalization  
3. Notional normalization  
4. Price max recalculation based on partition ticks

## Utility Methods
- `get_symbol()` — returns `"BASE/QUOTE"`
- `debug_dump()` — prints all fields
- `to_string()` — returns formatted string

## Example

```cpp
Instrument btcusd{
    "BTC", "USD", "BTC/USD",
    0.01, 0.0001,
    2, 4,
    200000.0, 100.0,
    0.0001, 5.0,
    "online"
};
auto normalized = btcusd.normalize(1'000'000);
```

---
 
## Related components

[`matching_engine::Manager`](../manager.md)
[`matching_engine::OrderBook`](../order_book.md)
[`matching_engine::OrderIdMap`](../order_id_map.md)
[`matching_engine::OrderPool`](../order_pool.md)
[`matching_engine::PartitionPool`](../partitions.md)
[`matching_engine::PriceLevelStore`](../price_level_store.md)
[`matching_engine::Telemetry`](../telemetry.md)

[`matching_engine::conf::NormalizedInstrument`](./normalized_instrument.md)
[`matching_engine::conf::PartitionPlan`](./partition_plan.md)

---

👉 Back to [`Manager - Matching Engine Orchestrator`](../manager.md)
