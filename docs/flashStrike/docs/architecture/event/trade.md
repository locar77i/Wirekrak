# FlashStrike Trade Event Format (64-Byte Compact Representation)

**File:** `flashstrike/event/trade.hpp`  
**Size:** 64 bytes (exact, cache-line aligned)  
**Purpose:** Internal trade event emitted by the matching engine after every execution.

---

## Overview

`flashstrike::event::Trade` is the canonical representation of an executed trade within the FlashStrike matching engine.  
Each trade records:

- maker and taker order identity  
- matched price and quantity  
- execution timestamp  
- execution-side information  
- fee accounting  

It is designed to be:

- **binary-stable**  
- **cache-efficient**  
- **zero-cost to copy / serialize / record**  
- **ideal for ring buffers and lock-free queues**  

Like all FlashStrike hot-path structures, it is **exactly 64 bytes** to fit in a single CPU cache line.

---

## 1. Layout Summary (64 bytes)

| Offset | Size | Field | Description |
|--------|-------|--------|-------------|
| 0      | 8B   | `seq_num` | Monotonic engine sequence number |
| 8      | 4B   | `maker_order_id` | ID of passive (resting) order |
| 12     | 4B   | `taker_order_id` | ID of aggressive order |
| 16     | 4B   | `maker_user_id` | Account owning the passive order |
| 20     | 4B   | `taker_user_id` | Account owning the aggressive order |
| 24     | 8B   | `price` | Execution price (scaled integer) |
| 32     | 8B   | `qty` | Execution quantity (scaled integer) |
| 40     | 8B   | `ts_engine_ns` | Engine timestamp (monotonic clock) |
| 48     | 4B   | `maker_fee` | Fee applied to maker |
| 52     | 4B   | `taker_fee` | Fee applied to taker |
| 56     | 1B   | `taker_side` | Side of taker order (BUY or SELL) |
| 57–63  | 7B   | `pad_[]` | Padding → must reach 64 bytes |

The padding ensures:

- stable ABI  
- predictable memory access behavior  
- no hidden padding inserted by the compiler  

---

## 2. Design Rationale

### ✔ Exactly one cache line  
Perfect for:

- high-frequency trade event queues  
- telemetry logging  
- downstream strategy callbacks  

### ✔ Trivially copyable  
The engine can:

- write trades to a ring buffer  
- record them into WAL  
- batch-copy to telemetry pipes  

…without running constructors or destructors.

### ✔ Stable binary format  
Safe for:

- file persistence  
- shared memory consumer processes  
- streaming to analytics systems  

### ✔ Predictable memory access  
All fields are laid out explicitly, with compiler offsets enforced via static asserts.

---

## 3. Field Semantics

### `seq_num`  
Global trade sequence number ensuring strict ordering of all executions.

### Maker/Taker Info  
The trade indicates:

- which order *rested* and provided liquidity (maker)  
- which order *crossed* the book (taker)  
- corresponding user accounts  

### `price` and `qty`  
Always stored as **scaled integer units**, matching the instrument’s normalization rules.

### `ts_engine_ns`  
Monotonic engine timestamp captured from `monotonic_clock::now_ns()`:

- strictly increasing  
- never repeats  
- resistant to system clock drift  

### Fees  
Per-participant fee charges.

### `taker_side`  
Represents the *aggressor side*:

- `Side::BID` → buy order takes liquidity  
- `Side::ASK` → sell order takes liquidity  

---

## 4. Constructors

### Default constructor  
Zero-initializes all fields.

### Full constructor  
Allows building complete trade records in one step:

```cpp
Trade(seq, maker_oid, taker_oid,
      maker_uid, taker_uid,
      p, q, ts, mfee, tfee, tside);
```

Used by the matching engine hot-path after an execution.

---

## 5. Debugging Helper

```cpp
void debug_dump(const Trade& e, std::ostream& os)
```

Prints a single-line summary including price, quantity, side, and IDs.  
Useful for debugging, WAL replay verification, and test harnesses.

---

## 6. ABI & Safety Guarantees

### Verified invariants

```cpp
static_assert(sizeof(Trade) == 64);
static_assert(alignof(Trade) == 64);
static_assert(std::is_trivially_copyable_v<Trade>);
static_assert(std::is_standard_layout_v<Trade>);
```

Meaning:

- no compiler padding surprises  
- safe for shared memory and DMA  
- compatible with `memcpy` and zero-copy append  
- stable binary representation  

Essential for ultra-low-latency trade dispatching.

---

## 7. Example Usage

### Emitting a trade from the matching engine

```cpp
Trade t(
    next_seq++,
    maker_oid,
    taker_oid,
    maker_uid,
    taker_uid,
    match_price,
    match_qty,
    monotonic_clock::instance().now_ns(),
    maker_fee,
    taker_fee,
    taker_side
);

trades_ring.push(t);
```

### Debug output

```cpp
t.debug_dump(t, std::cout);
```

---

## 8. Summary

The **Trade** event structure is:

- **64 bytes, cacheline optimized**  
- **binary-stable and trivially copyable**  
- **fully deterministic and replay-safe**  
- **ideal for high-frequency trade emission paths**  
- **a core part of FlashStrike’s market data and telemetry pipeline**  

Its design ensures minimal latency overhead during order matching and enables efficient downstream processing.

