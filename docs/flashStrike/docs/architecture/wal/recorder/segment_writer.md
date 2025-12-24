# FlashStrike WAL SegmentWriter — Architecture & Responsibilities

**File:** `flashstrike/wal/recorder/segment_writer.hpp`  
**Component:** Write-Ahead Log (WAL) Persistence Layer  
**Role:** Low-level storage writer for WAL segments

---

## Overview

`flashstrike::wal::recorder::SegmentWriter` is the core component responsible for **writing the engine’s event stream into persistent WAL segment files**.  
It transforms high-frequency in-memory events into a durable, verifiable on-disk log.

SegmentWriter manages:

- WAL file creation / opening  
- memory-mapped block writing  
- header maintenance  
- block finalization  
- chained checksums  
- safe close / sync / cleanup  

It is optimized for **ultra-low-latency append operations** in a single-producer pipeline.

---

## WAL Segment Structure (Recap)

A WAL segment file contains:

```
┌─────────────────────────────────────────────┐
│ Segment Header (64 bytes)                   │
├─────────────────────────────────────────────┤
│ Block 0 (Header + Events[WAL_BLOCK_EVENTS]) │
├─────────────────────────────────────────────┤
│ Block 1 (Header + Events[...])              │
├─────────────────────────────────────────────┤
│ ...                                         │
└─────────────────────────────────────────────┘
```

The writer appends events into in-memory blocks, finalizes them, and stores them via **memory-mapped I/O (mmap)** for minimal syscall overhead.

📄 **Detailed spec:**  
👉 [`WAL Storage model`](../segment_overview.md)

---

## Responsibilities of SegmentWriter

### ✔ Open and initialize a WAL segment  
- Create new segment (`open_new_segment()`), preventing accidental overwrites  
- Map file into memory (`mmap`)  
- Initialize the segment header  
- Prepare for high-throughput appends  

---

### ✔ Append events at high speed  

Every call to:

```cpp
append(const RequestEvent& ev)
```

Performs:

1. Add event to active block  
2. Update block header first/last event IDs  
3. Update segment header event counters  
4. When full → finalize + write block  

This method forms the **critical hot path** of WAL persistence.

---

### ✔ Flush partially filled blocks  

Used before:

- segment rotation  
- shutdown  
- handoff to another thread  

```cpp
flush_partial();
```

---

### ✔ Finalize and close segment  

```cpp
close_segment(bool sync);
```

Performs:

- flushing final block  
- writing final header fields  
- checksumming  
- fsync (optional)  
- unmapping  
- cache eviction hints  

Ensures **crash-consistent persistence**.

---

## Integrity Guarantees

### ✔ Self-contained segment metadata  
SegmentWriter ensures:

- first/last event IDs  
- event count  
- block count  
- timestamps  

are always correct.

### ✔ Three checksum layers  

1. **Block checksum** — ensures event payload correctness  
2. **Chained checksum** — forward-secure integrity chain  
3. **Header checksum** — structural validation  

These make the WAL resistant to:

- torn writes  
- partial flushes  
- tampering  
- silent corruption  

---

## Thread-Safety Model

SegmentWriter is **not thread-safe** by design.

Correct usage pattern:

- One thread performs all `append()` operations  
- Ownership may be passed to another thread when the segment is closed or rotated  
- But **never two threads concurrently**  

This eliminates the need for locks on the hot path.

---

## Memory-Mapped I/O Design

SegmentWriter uses `mmap` to write events:

- appending is simply writing to memory  
- the kernel handles batching to disk  
- page faults can be pre-faulted with `touch()`  
- large writes stay extremely cheap  

Optional flush paths:

```cpp
flush(true);   // fsync to disk  
flush(false);  // async msync  
```

---

## Block Finalization Logic

Before a block is written:

```
block.finalize(block_index, prev_chained)
```

This computes:

- block checksum  
- chained checksum  
- header checksum  

After writing:

- advance block index  
- update segment header block count  
- reset block buffer  

---

## Restoring Existing Segments

When reopening a WAL for append:

```cpp
open_existing_segment();
```

This performs:

1. Map the file  
2. Validate segment integrity  
3. Walk blocks to find the exact append point  
4. Restore:  
   - block_index  
   - prev_chained  
   - partially filled block  
   - bytes_written  

This allows **resume-on-crash** semantics.

---

## Automatic Cleanup Logic

In the destructor, the writer will:

- close valid segments safely  
- but *remove* invalid or empty files  

The system therefore never leaves behind:

- zero-event segments  
- truncated garbage  
- partially written structural headers  

---

## Invariants Enforced

- `segment_size = header + N * block_size`  
- monotonic block indices  
- event ordering preserved  
- every written block has valid checksums  
- final header written atomically  

These invariants make the WAL:

- replayable  
- durable  
- audit-safe  
- free from structural ambiguity  

---

## Typical Usage Flow

### Creating a new segment

```cpp
SegmentWriter writer(dir, filename, num_blocks, metrics);

writer.open_new_segment(segment_index);
writer.touch();

for each event:
    writer.append(event);

writer.flush_partial();
writer.close_segment(true);
```

### Reopening for continuation

```cpp
writer.open_existing_segment();
// resumes appending from last valid block
```

---

## Summary

`SegmentWriter` is the **persistence backbone** of FlashStrike’s Write-Ahead Log system.

It provides:

- ultra-low latency append path  
- memory-mapped block writing  
- strong integrity verification  
- crash-safe finalization  
- zero-copy operations  
- efficient page/cache management  

It ensures every event processed by the matching engine is safely and deterministically persisted.

---
 
## Related components

[`recorder::Manager`](./manager.md)
[`recorder::Meta`](./meta.md)
[`recorder::Telemetry`](./telemetry.md)

[`recorder::worker::MetaCoordinator`](./worker/meta_coordinator.md)
[`recorder::worker::SegmentPreparer`](./worker/segment_preparer.md)
[`recorder::worker::SegmentMaintainer`](./worker/segment_maintainer.md)

---

👉 Back to [`WAL Recorder System — Overview`](../recorder_overview.md)
