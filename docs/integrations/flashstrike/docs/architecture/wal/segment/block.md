# WAL Segment Block Documentation  
**File:** `flashstrike/wal/segment/block.hpp`  
**Component:** Full WAL block (header + events)  
**Alignment:** 64 bytes (cache-line aligned)

---

## Overview

The `flashstrike::wal::segment::Block` structure represents the **fundamental persistence unit** within the Flashstrike Write-Ahead Log (WAL).  
Each WAL block contains:

1. A `BlockHeader` defining metadata, checksums, event range, and block identity  
2. A fixed-size array of `RequestEvent` entries  

Blocks are designed for:

- **High-throughput persistence**
- **Zero-overhead serialization**
- **Fast, sequential WAL replay**
- **Checksum‑based corruption detection**
- **Low-latency I/O**

Because the block is trivially copyable and cache-aligned, it can be written and read using raw `memcpy()` or direct filesystem writes without transformation.

---

## Block Structure

```cpp
struct alignas(64) Block {
    BlockHeader header;
    RequestEvent events[WAL_BLOCK_EVENTS];
};
```

### Key properties

| Property | Description |
|---------|-------------|
| **64‑byte alignment** | Ensures consistent cache behavior |
| **Trivial & standard layout** | Safe for `memcpy`, mmap, DMA, and binary replay |
| **Fixed size** | Simplifies WAL segment layout |
| **Includes local + chained checksums** | Enables robust corruption detection |

---

## Purpose of a Block

A WAL block stores a contiguous batch of events. Together with its header, it provides:

### ✔ Integrity  
Local checksum validates only this block’s event region.  
Chained checksum links this block to the previous one, forming a tamper‑evident chain.

### ✔ Replay Semantics  
WAL replay relies on:

- block index  
- event count  
- event ID range  
- chained checksum sequence  

to reconstruct the exact event history.

### ✔ Fast Scanning  
Since block size is fixed, WAL readers can jump to any block via:

```
offset = sizeof(Header) + block_index * sizeof(Block)
```

---

## Reset Operations

### `reset()`
Clears the entire block, including events and header fields.

### `reset_pad()`
Clears only padding inside the header and events.  
(This is a future‑proof pattern to avoid leaking uninitialized padding on disk.)

---

## Checksum System

WAL blocks implement **two distinct checksum types**:

---

### 1. Local checksum  
Validates the event payload only.

```cpp
compute_block_checksum(events, header.event_count())
```

This covers:

- event IDs  
- event kinds  
- embedded order data  

It **does not** depend on any previous block.

---

### 2. Chained checksum  
Creates a forward‑chained integrity link:

```cpp
compute_chained_checksum(events, count, prev_chained)
```

This prevents:

- reordering events across blocks  
- removing a block  
- replacing a block  

without detection.

This is the WAL equivalent of a **hash chain** or forward‑secure log.

---

## Block Finalization

Before persisting the block, the engine calls:

```cpp
finalize(block_index, prev_chained);
```

This performs:

1. Local checksum computation  
2. Chained checksum computation  
3. Header finalization via `BlockHeader::finalize()`  

After finalization, the block is ready to be written to disk.

---

## Structural Validation

`validate_data()` performs:

### ✔ Header validation  
Delegates to `BlockHeader::validate_data()`.

### ✔ Event ID monotonicity  
Events must satisfy:

```
events[i].event_id > events[i - 1].event_id
```

### ✔ Logical range validation  
Ensures:

```
first_event_id <= last_event_id
```

Any violation implies WAL corruption or an incomplete write.

---

## Checksum Validation

`validate_checksums(prev_chain)`:

### Step 1 — local checksum
Detects corruption inside the event array.

### Step 2 — chained checksum
Ensures continuity with the previous block.

Possible error results:

- `Status::BLOCK_CHECKSUM_MISMATCH`
- `Status::CHAINED_CHECKSUM_MISMATCH`

---

## Full Verification

`verify(prev_chained)` performs both:

1. Structural checks  
2. Checksum checks  

Return values:

- `Status::OK`  
- `Status::SEGMENT_POSSIBLY_CORRUPTED`  
- `Status::BLOCK_CHECKSUM_MISMATCH`  
- `Status::CHAINED_CHECKSUM_MISMATCH`  

This function is used during WAL replay.

---

## Serialization

Because the block is trivial and standard‑layout, persistence uses:

```cpp
serialize(void* dest);
deserialize(const void* src);
```

Both rely on raw `memcpy()`, providing:

- zero‑copy load/store  
- maximum I/O throughput  
- byte‑exact fidelity for WAL replay  

---

## Size Helpers

```cpp
static constexpr size_t byte_size()
```

Returns the exact binary footprint of a block:

```
sizeof(BlockHeader) + WAL_BLOCK_EVENTS * sizeof(RequestEvent)
```

Used when preallocating segments and scanning WAL files.

---

## Compile‑Time Guarantees

Static assertions ensure:

- **64‑byte alignment**
- correct field offsets (`header` at 0, `events` immediately after)
- POD semantics  
- exact struct size stability  

These eliminate the risk of ABI drift, which would break WAL compatibility.

---

## Example Usage

### Writing:

```cpp
Block blk;
blk.header.set_event_count(n);
blk.header.set_first_event_id(events[0].event_id);
blk.header.set_last_event_id(events[n - 1].event_id);

// finalize before persisting
blk.finalize(block_index, prev_chain);

// write to file
write(fd, &blk, sizeof(Block));
```

### Reading / Replay:

```cpp
Block blk;
blk.deserialize(ptr);

Status s = blk.verify(prev_chain);
if (s != Status::OK) {
    // corruption detected
}
```

---

## Summary

The WAL Block is a **high‑integrity, high‑performance persistence structure** supporting:

- fixed‑size binary layout  
- batch event storage  
- fast checksums  
- hash‑chained durability  
- zero‑copy serialization  
- replay‑safe semantics  

It is a core foundation of Flashstrike’s **durable event log**, designed for reliability, replay determinism, and extreme performance.

---

## Related components

[`segment::Header`](./header.md)
[`segment::BlockHeader`](./block_header.md)

---

👉 Back to [`WAL Storage Architecture — Overview`](../segment_overview.md)
