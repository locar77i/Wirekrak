# WAL Recovery Telemetry — SegmentReader Metrics

This document describes the **SegmentReader telemetry subsystem** used in the Flashstrike WAL Recovery engine.  
It captures fine‑grained performance, latency, and integrity‑failure statistics for each stage of WAL segment processing.

---

## Overview

`telemetry::SegmentReader` provides detailed observability of:

- Segment open/close operations  
- Full-segment checksum validation  
- Block-level and chained checksum failures  
- Sparse index construction timing  
- Event seek latency  

Every metric is cache‑line aligned (64 bytes) to prevent false sharing and ensure deterministic performance.

---

## Telemetry Structure

```cpp
struct alignas(64) SegmentReader {
    stats::operation64 open_segment;
    stats::operation64 close_segment;
    stats::operation64 verify_segment;

    counter64 total_header_checksum_failures;
    counter64 total_block_checksum_failures;
    counter64 total_chained_checksum_failures;
    counter64 total_validation_failures;

    stats::operation64 build_index;
    stats::duration64  seek_event;
};
```

### What each metric records

| Metric | Description |
|--------|-------------|
| **open_segment** | Time to memory-map a WAL segment and load its header |
| **close_segment** | Time to unmap and close a segment file |
| **verify_segment** | Full-segment integrity validation (header checksum, block checksums, chained checksum) |
| **total_header_checksum_failures** | Number of corrupted block headers encountered |
| **total_block_checksum_failures** | Count of failed per-block checksums |
| **total_chained_checksum_failures** | Number of chained checksum mismatches across blocks |
| **total_validation_failures** | Any other validation errors |
| **build_index** | Time to generate sparse block index |
| **seek_event** | Latency of seeking to the nearest event ID |

---

## SegmentReaderUpdater

`SegmentReaderUpdater` provides optimized, low-overhead hooks for updating metrics:

```cpp
class SegmentReaderUpdater {
public:
    void on_open_segment(uint64_t start_ns, Status status);
    void on_close_segment(uint64_t start_ns, Status status);
    void on_verify_segment(uint64_t start_ns, Status status);
    void on_build_index(uint64_t start_ns, Status status);
    void on_seek_event(uint64_t start_ns);
};
```

### Integrity Failure Categorization

If verification fails, telemetry increments the correct counter:

- `HEADER_CHECKSUM_MISMATCH`
- `BLOCK_CHECKSUM_MISMATCH`
- `CHAINED_CHECKSUM_MISMATCH`
- `SEGMENT_CORRUPTED`
- `SEGMENT_POSSIBLY_CORRUPTED`

---

## Dump Output (Example)

```
[Segment Reader Metrics] Snapshot:
-----------------------------------------------------------------
 Open segment  : count=14 p50=0.12ms p99=0.34ms
 Close segment : count=14 p50=0.05ms p99=0.09ms
 Verify segment: count=14 p50=120ms  p99=220ms
 - Header checksum failures : 0
 - Block checksum failures  : 2
 - Chained checksum failures: 0
 - Validation failures      : 0
 Build index   : count=14 p50=0.04ms p99=0.12ms
 Seek event    : count=200000 p50=0.18µs p99=0.41µs
-----------------------------------------------------------------
```

---

## Compile‑Time Guarantees

The class enforces:

```cpp
static_assert(sizeof(SegmentReader) % 64 == 0);
static_assert(alignof(SegmentReader) == 64);
static_assert(offsetof(SegmentReader, open_segment) % 64 == 0);
static_assert(offsetof(SegmentReader, seek_event)  % 64 == 0);
```

These ensure:

- No false sharing  
- Full alignment with CPU cache lines  
- Predictable structure layout for serialization and mmap safety  

---

## File Purpose Summary

`telemetry::SegmentReader` is the **primary observability mechanism** for:

- WAL segment validation  
- Block integrity outcomes  
- Event replay navigation latency  
- Internal recovery-stage profiling  

It supports high-throughput recovery (>10M events/sec) and provides essential visibility for diagnosing performance, corruption, or I/O issues.

---

## Related components

[`recovery::telemetry::Manager`](./manager.md)

[`recovery::telemetry::worker::SegmentPreloader`](./worker/segment_preloader.md)

---
 
👉 Back to [`Telemetry – WAL Recovery Metrics Aggregator`](../telemetry.md)
