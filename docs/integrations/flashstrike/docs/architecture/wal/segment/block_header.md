# WAL Block Header Documentation  
**File:** `flashstrike/wwal/segment/block_header.hpp`  
**Size:** 64 bytes (cache-line aligned)  
**Component:** Metadata header for a single WAL block  

---

## Overview

`flashstrike::wal::segment::BlockHeader` defines the **compact, fixed-size, cache-aligned metadata record** that precedes every WAL block.  
It provides:

- event range indexing  
- block identification  
- checksums (local + chained + header checksum)  
- endian-stable persistence  
- structural validation  

At exactly **64 bytes**, the header fits in a single CPU cache line, optimizing:

- sequential WAL scanning  
- high-frequency block writes  
- replay and validation logic  

It is also **POD**, **trivially copyable**, and **safe for raw I/O operations**.

---

## Binary Layout Summary

| Offset | Size | Field | Description |
|--------|------|--------|-------------|
| 0      | 8B  | `first_event_id_le` | ID of the first event in the block |
| 8      | 8B  | `last_event_id_le` | ID of the last event |
| 16     | 8B  | `block_checksum_le` | XXH64 of the block's events only |
| 24     | 8B  | `chained_checksum_le` | XXH64(events + previous chain) |
| 32     | 8B  | `checksum_le` | Header checksum (excluding this field) |
| 40     | 4B  | `block_index_le` | Block index inside the segment |
| 44     | 2B  | `event_count_le` | # of valid events in this block |
| 46     | 18B | `pad_[]` | Reserved for alignment → total 64 bytes |

➡️ All fields are stored in **little-endian** to ensure platform-independent WAL replay.

---

## Responsibilities

The BlockHeader encapsulates:

### ✔ Block identification
- which block this is (`block_index`)
- how many events it contains

### ✔ Event ID range
Used for:

- sequential replay  
- indexing  
- verifying monotonicity  

### ✔ Integrity protection
Three checksums:

| Type | Purpose |
|------|---------|
| **Block checksum** | Integrity of event payload only |
| **Chained checksum** | Tamper-evident link to previous block |
| **Header checksum** | Ensures header structural integrity |

### ✔ Structural correctness
Ensures logical consistency before replaying a block.

---

## Endian-Safe Accessors

All getters convert from little-endian values.  
All setters convert values to little-endian.

Ensures WAL logs remain portable across CPU architectures (x86, ARM, etc.).

Example:

```cpp
uint64_t last_event_id() const noexcept;
uint16_t event_count() const noexcept;
uint32_t block_index() const noexcept;
```

---

## Reset Operations

### `reset()`
Clears all fields to zero.

### `reset_pad()`
Clears the padding region (`pad_[]`), ensuring no uninitialized bytes are written to disk.

---

## Header Checksum System

Each header includes a dedicated checksum computed via:

```cpp
compute_checksum(*this)
```

### How checksum works:

- The header is hashed in two parts:
  1. bytes **before** the checksum field  
  2. bytes **after**, seeded with the first hash  

This avoids self-dependency while still validating all structural metadata.

### Checksum validation

```cpp
validate_checksum()
```

Returns `true` if the checksum matches the expected value.

---

## Finalization

Before writing a WAL block, the engine calls:

```cpp
finalize(block_index, block_checksum, chained_checksum);
```

This operation:

1. Sets block index  
2. Stores block and chained checksums  
3. Computes and sets the header checksum  

After finalization, the header is ready for durable WAL persistence.

---

## Structural Validation

`validate_data()` ensures:

### ✔ Event count is valid

```
1 ≤ event_count ≤ WAL_BLOCK_EVENTS
```

### ✔ Event ID range is correct

- both IDs must be non-zero  
- `first_event_id ≤ last_event_id`  

### ✔ Block index is within expected bounds

```
block_index ≤ MAX_BLOCKS
```

Any violation indicates structural corruption.

---

## Compile-Time ABI Guarantees

The implementation uses strict static assertions:

- `sizeof(BlockHeader) == 64`  
- `alignof(BlockHeader) == 64`  
- field offset checks for every member  
- POD constraints (`trivial` and `standard_layout`)  

These safeguards protect the WAL format against unintended binary layout drift.

---

## Example Usage

### During WAL writing:

```cpp
BlockHeader hdr;
hdr.set_first_event_id(events[0].event_id);
hdr.set_last_event_id(events[n - 1].event_id);
hdr.set_event_count(n);

hdr.finalize(block_idx, block_checksum, chained_checksum);
```

### During WAL replay:

```cpp
if (!hdr.validate_data()) {
    // structural corruption
}

if (!hdr.validate_checksum()) {
    // header checksum mismatch
}
```

---

## Summary

The `BlockHeader` is a **critical metadata structure** that ensures:

- compact, cache-friendly layout  
- strong corruption detection (3 checksum layers)  
- deterministic event ordering  
- endian-stable persistence  
- compatibility across releases  

It forms the upper metadata tier of each WAL block, providing the reliability and auditability required for Flashstrike’s **high-performance event log**.

---

## Related components

[`segment::Header`](./header.md)
[`segment::Block`](./block.md)

---

👉 Back to [`WAL Storage Architecture — Overview`](../segment_overview.md)
