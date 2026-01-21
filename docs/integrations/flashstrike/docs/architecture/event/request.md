# Flashstrike Request Event (Live Engine Event Format)

**File:** `flashstrike/event/request.hpp`  
**Size:** 64 bytes (exact, cache-line aligned)  
**Component:** Core in-memory event format used by the Flashstrike matching engine.

---

## Overview

`flashstrike::event::Request` represents the **live operational event** consumed by the matching engine.  
Every incoming order action—new order, cancel, modify—is normalized into this compact 64-byte struct.

Design goals:

- **One event = one cache line**  
- **Zero false sharing** in multi-core paths  
- **Lock-free ring-buffer friendly**  
- **Deterministic memory access**  
- **Strictly monotonic ordering (via event_id)**  

This struct is used *only for live processing*. The WAL uses a compatible but distinct serialized format.

---

## 1. Layout Summary (64 bytes)

| Offset | Size | Field | Description |
|--------|-------|--------|------------|
| 0      | 8B   | `event_id` | Strictly increasing ID; global ordering anchor |
| 8      | 8B   | `timestamp` | Nanosecond timestamp (exchange or system clock) |
| 16     | 8B   | `price` | Price of order (scaled integer) |
| 24     | 8B   | `quantity` | Quantity (scaled integer) |
| 32     | 4B   | `user_id` | Originating user |
| 36     | 4B   | `order_id` | Unique order identifier |
| 40     | 1B   | `type` | Request type (New, Cancel, Modify…) |
| 41     | 1B   | `order_type` | Limit, Market, IOC, etc. |
| 42     | 1B   | `side` | Buy or Sell |
| 43–63  | 21B  | `pad_[]` | Padding to align struct to 64 bytes |

The explicit padding ensures:

- contiguous 64-byte cache-line occupancy  
- no out-of-struct padding leakage  
- stable ABI across compilers  

---

## 2. Why 64 Bytes?

This size is intentional and critical for ultra-low-latency design.

### ✔ Exactly one cache line  
Ensures every Request fetch is a **single aligned load** with no cross-line pollution.

### ✔ Zero false sharing  
Multiple threads pushing/popping events in a lock-free ring buffer never collide.

### ✔ Predictable memory access  
Matching-engine hot paths rely on deterministic access patterns.

### ✔ SIMD and prefetch-friendly  
Many vectorized operations assume 64B blocks.

### ✔ Perfect fit for modern CPU prefetchers  
Hardware prefetchers optimize for 64B stepping.

---

## 3. Field Responsibilities

### `event_id`  
Strictly increasing sequence number assigned at ingest.  
Used for: ordering, WAL linking, replay integrity, tracing.

### `timestamp`  
Nanosecond-resolution timestamp, typically:

- exchange clock  
- monotonic local clock  
- hybrid trading clock  

### `price` and `quantity`  
Stored as **scaled integers** to avoid floating-point rounding.

### `user_id`, `order_id`  
Identifiers mapping back to account/session-level context.

### `type`  
Enumerated request type: NEW, CANCEL, MODIFY, etc.

### `order_type`  
Enumerated order type: LIMIT, MARKET, IOC, FOK, POST-ONLY, etc.

### `side`  
BUY or SELL.

---

## 4. Reset Operations

### `reset()`  
Clears the entire event:

```cpp
inline void reset() noexcept {
    std::memset(this, 0, sizeof(Request));
}
```

### `reset_pad()`  
Clears padding bytes safely, preventing uninitialized memory writes.

---

## 5. ABI & Safety Guarantees

Strict compile-time validations ensure:

### ✔ Correct size  
`sizeof(Request) == 64`

### ✔ 64-byte alignment  
Guarantees cache-line locality.

### ✔ Standard layout  
Safe for low-level serialization, shared memory, and ring buffers.

### ✔ Trivial and trivially copyable  
- safe for `memcpy`  
- no constructors/destructors  
- deterministic binary layout  
- compatible with DMA and mmap  

### ✔ Verified field offsets  
Ensures no compiler-inserted padding.

---

## 6. Usage Example

### Creating a Request

```cpp
event::Request req;
req.event_id = next_id++;
req.timestamp = clock.now_ns();
req.price = price_scaled;
req.quantity = qty_scaled;
req.user_id = uid;
req.order_id = oid;
req.type = RequestType::NEW_ORDER;
req.order_type = OrderType::LIMIT;
req.side = Side::BUY;
```

### Reset for reuse

```cpp
req.reset();
```

---

## 7. Summary

The **Request** struct is foundational to Flashstrike’s ultra-low-latency event pipeline.

It is:

- **64 bytes, cacheline aligned**  
- **Predictable and binary-stable**  
- **Optimized for lock-free queues and high-frequency ingestion**  
- **Designed for deterministic behavior and strict ordering**  
- **The canonical in-memory representation of incoming trading events**

It forms the atomic unit of work inside the Flashstrike matching engine’s hot path.

