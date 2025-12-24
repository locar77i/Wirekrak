# WAL Segment Header Documentation  
**File:** `flashstrike/wal/segment/header.hpp`  
**Size:** 64 bytes (cache-line aligned)  
**Purpose:** Durable metadata for Write-Ahead Log (WAL) segments

---

## Overview

The `flashstrike::wal::segment::Header` struct defines the **fixed-size, cache-aligned metadata block** stored at the beginning of every WAL segment file.  
It enables:

- log replay  
- corruption detection  
- fast seeking and scanning  
- version compatibility  
- structural validation  

Every WAL segment begins with exactly **64 bytes** of header metadata.

This struct is:

- **trivially copyable**  
- **standard layout**  
- **exactly 64 bytes**  
- **64-byte aligned**  
- safe to `memcpy()` directly  

---

## Layout Summary

| Offset | Size | Field | Description |
|-------|------|--------|-------------|
| 0     | 2B   | `magic_le` | Magic identifier (`WAL_MAGIC`) |
| 2     | 1B   | `version_le` | WAL format version |
| 3     | 1B   | `header_size_le` | Always sizeof(Header) |
| 4     | 4B   | `segment_index_le` | Sequential index of this WAL segment |
| 8     | 4B   | `block_count_le` | Number of blocks in the segment |
| 12    | 4B   | `event_count_le` | Number of events written |
| 16    | 8B   | `first_event_id_le` | First event ID stored |
| 24    | 8B   | `last_event_id_le` | Last event ID stored |
| 32    | 8B   | `created_ts_ns_le` | Creation timestamp (ns) |
| 40    | 8B   | `closed_ts_ns_le` | Close timestamp (ns) |
| 48    | 8B   | `checksum_le` | Header checksum |
| 56    | 8B   | `last_chained_checksum_le` | Cross-segment anchor |

➡️ The entire header is stored in **little-endian form**, enabling consistent cross-platform replay.

---

## Responsibilities

The WAL header ensures:

### ✔ Segment integrity  
Through a fast 64-bit **XXH64** checksum.

### ✔ Structural validation  
Version, size, magic, and field constraints.

### ✔ Replay anchoring  
Replay systems rely on:

- `segment_index`
- `first_event_id`
- `last_event_id`
- `last_chained_checksum`

to reconstruct the full event chronology.

### ✔ Fast seeking  
Metadata allows scanning WAL segments without parsing all blocks.

### ✔ Lightweight durability  
At 64 bytes, the header fits in a single cache line and can be written atomically by FS implementations.

---

## Accessors (Endian-safe)

All getters convert from little-endian:

```cpp
uint32_t block_count() const noexcept
uint64_t first_event_id() const noexcept
uint64_t closed_ts_ns() const noexcept
...
```

All setters store values in little-endian form.

This allows:

- platform agnostic persistence  
- safe WAL replay across architectures  

---

## Checksum System

The header includes a **64-bit XXH64 checksum**, computed via:

```cpp
Header::compute_checksum()
```

### How it works:

1. Hash bytes *before* `checksum_le`  
2. Use that hash as the seed  
3. Hash bytes *after* `checksum_le`  

This avoids circular dependency and is extremely fast for a 64-byte struct.

### Validation:

```cpp
validate_checksum()
```

Returns:

- `Status::OK`
- `Status::HEADER_CHECKSUM_MISMATCH`

---

## Structural Validation

`validate_data()` checks:

- correct magic  
- correct version  
- correct header size  

Failures are logged via `DBG`.

`verify()` combines:

1. checksum validation  
2. structural validation

Final result is one of the durable `Status` codes.

---

## Lifecycle Operations

### `reset()`  
Clears all fields to zero.

### `finalize(chained)`  
Executed when closing a WAL segment:

- sets `closed_ts_ns` (using `monotonic_clock`)
- sets chained checksum (anchor to previous segment)
- recomputes header checksum

### Serialization helpers

```cpp
serialize(void* dest)
deserialize(const void* src)
```

Perform raw 64-byte memcpy operations.

---

## Segment Sizing

`segment_size()` returns the total size implied by:

```
header + block_count * sizeof(Block)
```

Used during replay and log scanning.

---

## Compile-Time Guarantees

Static assertions ensure:

- exact size: `sizeof(Header) == 64`  
- exact alignment: `alignof(Header) == 64`  
- correct offset for every field  
- POD semantics: trivially copyable, standard layout  

These prevent silent ABI drift—which could corrupt WAL persistence.

---

## Example Usage

```cpp
Header hdr;
hdr.set_magic(WAL_MAGIC);
hdr.set_version(WAL_VERSION);
hdr.set_header_size(sizeof(Header));
hdr.set_segment_index(42);
hdr.set_created_ts_ns(monotonic_clock::instance().now_ns());

// Finalize when closing segment
hdr.finalize(previous_segment_checksum);
```

---

## Summary

The WAL `Header` struct is:

- **Compact, cache-aligned, endian-stable**
- **Designed for durability and correctness**
- **Optimized for extremely low latency (XXH64 hot-path hashing)**
- **Fully verifiable and replay-safe**
- **A foundational component of FlashStrike’s persistent event log system**

It acts as the authoritative metadata record for every segment, enabling
efficient replay, corruption detection, and sequential log processing.

---

## Related components

[`segment::Block`](./block.md)
[`segment::BlockHeader`](./block_header.md)

---

👉 Back to [`WAL Storage Architecture — Overview`](../segment_overview.md)
