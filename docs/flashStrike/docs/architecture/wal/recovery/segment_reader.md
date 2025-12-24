# FlashStrike WAL — `SegmentReader` Module Documentation

## Overview

`SegmentReader` is the high-performance **WAL (Write-Ahead Log) segment reader** used during recovery and replay in the FlashStrike system. It is designed around three major goals:

1. **Ultra-low-latency reads** via memory-mapped I/O (`mmap`).
2. **Strong integrity validation**, including:
   - Block integrity (checksum of the block)
   - Chained integrity (checksum across block boundaries)
3. **Efficient navigation** through WAL segments, with:
   - Sequential reading
   - Block-indexed seeking
   - Hybrid binary search + linear scan for precise event lookup

It is fully compatible with the block structure used by the WAL writer (`SegmentWriter`).

---

## Key Responsibilities

### ✔ Memory-Mapped WAL Access
Segments are opened via `mmap()` and read directly from virtual memory. This eliminates syscalls and copies during read operations.

### ✔ Full Segment Integrity Verification
When `open_segment()` is called:

- The entire file is checksum-validated.
- Block-level validation ensures:
  - Correct event count
  - Correct block index
  - Correct block checksum
- Chained-checksum validation ensures:
  - Cross-block continuity
  - Detection of truncated or tampered WAL data

### ✔ Sequential Streaming
The `next()` method provides high-throughput iteration:

```cpp
RequestEvent ev;
while (reader.next(ev)) {
    // Handle event
}
```

### ✔ Block-Level Indexed Seek
Seeking uses a two-phase strategy:

1. **Binary search** across a sparse index of WAL blocks
2. **Linear scan** within the block to find the exact event or nearest successor

This makes seeking extremely efficient, even for very large WAL segments.

---

## Public API Summary

### Constructor

```cpp
SegmentReader(const std::string& filepath,
              telemetry::SegmentReader& metrics);
```

### Opening and closing

```cpp
Status open_segment() noexcept;
Status close_segment() noexcept;
```

### Sequential read

```cpp
bool next(RequestEvent& ev) noexcept;
```

### Indexed seek

```cpp
bool seek(uint64_t event_id) noexcept;
void build_index() noexcept;
```

### Metadata Accessors

- `first_event_id()`
- `last_event_id()`
- `event_count()`
- `created_ts_ns()`
- `closed_ts_ns()`
- `filepath()`
- `is_valid()`

---

## Internal Architecture

### Memory Layout

```
+-----------------------+
| Segment Header        |
+-----------------------+
| Block 0               |
| - block header        |
| - 64 RequestEvent     |
+-----------------------+
| Block 1               |
+-----------------------+
| ...                   |
+-----------------------+
```

### Major Internal Components

| Component | Purpose |
|----------|---------|
| `open_file_()` | Opens and memory-maps the WAL segment |
| `verify_full_segment_integrity()` | Validates the entire segment |
| `read_block_at_offset_()` | Loads a WAL block into memory |
| `build_index_internally_()` | Builds a sparse block index |
| `seek_()` | Implements binary-search-based event seek |

---

## Seek Algorithm

FlashStrike uses a **hybrid algorithm**, optimized for WAL properties:

### 1. Sparse Index
Each block contributes:

- `first_event_id`
- `last_event_id`
- `file_offset`

This allows millions of events to be indexed with only a few thousand entries.

### 2. Binary Search
This finds the nearest block that may contain the target event.

### 3. Linear Scan
Inside a block, the target event is located by scanning the event array:

```cpp
for (i in events):
    if events[i].event_id >= target:
        return block[i]
```

This minimizes latency while keeping code complexity small.

---

## Telemetry Integration

`SegmentReader` hooks into:

- Open latency
- Close latency
- Verification time
- Seek latency
- Index build time

This provides visibility into slow segments and recovery performance.

---

## Error Handling & Robustness

`SegmentReader` is designed to recover gracefully from:

- Truncated segments
- Corrupted blocks
- Partial writes
- Invalid checksums
- Incorrect event counts

Corrupted segments fail fast and return:

```
Status::SEGMENT_CORRUPTED
Status::SEGMENT_POSSIBLY_CORRUPTED
```

---

## Lifespan Management

Destruction automatically unmaps and closes the file:

```cpp
~SegmentReader() { force_close_file_if_needed_(); }
```

This ensures cleanup even under exception pathways.

---

## Complete Feature Checklist

### Supported
✔ mmap access  
✔ Sequential reading  
✔ Indexed seeking  
✔ Full integrity verification  
✔ Chained checksum validation  
✔ Safe failure modes  
✔ Telemetry instrumentation  
✔ Minimal overhead per event  

### Not Included
✘ Random writes  
✘ Modification of WAL  
✘ Partial-block read/write  

---

# Summary

`SegmentReader` is a core component of FlashStrike’s WAL recovery pipeline, providing:

- **High performance** (zero-copy, low-latency memory scanning)
- **High reliability** (layered checksums, block validation)
- **Efficient navigation** (sparse indexing + hybrid search)

It ensures that WAL replay is both **fast enough for production restart** and **robust enough for financial-grade consistency**.

---

## Related components

[`recovery::Manager`](./manager.md)
[`recovery::Telemetry`](./telemetry.md)

[`recovery::worker::SegmentPreloader`](./worker/segment_preloader.md)

---

👉 Back to [`WAL Recovery System — Overview`](../recovery_overview.md)
