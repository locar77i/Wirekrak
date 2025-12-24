# WAL Segment Writer Telemetry — Documentation

## Overview

`flashstrike::wal::recorder::telemetry::SegmentWriter` provides **cache‑aligned, low‑overhead telemetry instrumentation** for the WAL segment writing pipeline.  
It records latencies, integrity failures, and operational statistics for:

- Creating new WAL segments  
- Opening existing WAL segments  
- Closing completed segments  
- Writing WAL blocks  
- Validating headers, block checksums, and chained checksums  

This telemetry block is tuned for **ultra‑low‑latency logging environments**, ensuring:

- Zero dynamic allocation  
- No locks on the hot path  
- 64‑byte cache‑line alignment to avoid false sharing  
- Trivially copyable metrics collection  

---

## Struct: `SegmentWriter`

```cpp
struct alignas(64) SegmentWriter {
    stats::operation64 open_new_segment;
    stats::operation64 open_existing_segment;
    stats::operation64 close_segment;
    stats::duration64  write_block;

    counter64 total_header_checksum_failures;
    counter64 total_block_checksum_failures;
    counter64 total_chained_checksum_failures;
    counter64 total_validation_failures;

    char pad_[64 - (4 * sizeof(counter64)) % 64];
};
```

### Key Properties

| Feature | Description |
|--------|-------------|
| **64‑byte alignment** | Ensures each metric group resides in an isolated cache line. |
| **operation64 counters** | Track `(start_ns → end_ns)` latency with success/failure bit. |
| **duration64 counters** | Measure execution time of non‑status operations (e.g., block write). |
| **Failure counters** | Incremented on checksum mismatch, corruption, or integrity issues. |
| **No virtual methods** | Entire type is trivially copyable and POD‑compatible. |

---

## Telemetry Captured

### 1. Segment Operations

| Metric | Meaning |
|--------|---------|
| `open_new_segment` | Time to initialize and create a new WAL segment file. |
| `open_existing_segment` | Time to reopen and validate an existing WAL segment. |
| `close_segment` | Time required to seal a WAL segment after rotation. |

### 2. Block Write Latency

`write_block` measures time spent writing a full WAL block  
(typically containing a batch of events).

This is one of the most important metrics for achieving predictable tail latency.

---

## Data Integrity Metrics

| Counter | Trigger Condition |
|---------|------------------|
| `total_header_checksum_failures` | Header checksum mismatch detected |
| `total_block_checksum_failures` | Individual block checksum mismatch |
| `total_chained_checksum_failures` | Linked block checksum mismatch (chain integrity) |
| `total_validation_failures` | General corruption or format violation |

Failures never crash the system; instead, they are captured for monitoring and operator alerts.

---

## Methods

### `copy_to()`
Copies all metrics atomically into another telemetry object.

Useful for exporting snapshots without slowing down the hot path.

### `dump(label, ostream)`
Prints human‑readable summary:

```
[Segment Writer Metrics] Snapshot:
Open new segment     : <avg / p50 / p99 / count>
Open existing segment: <...>
Close segment        : <...>
Verify segment:
- Header checksum failures : N
- Block checksum failures  : N
...
Write block          : <block latency distribution>
```

### Metrics Collection (Prometheus Style)

```cpp
template <typename Collector>
void collect(const std::string& prefix, Collector& collector) const;
```

Each metric is exported with prefix:

```
mc_wal_segment_writer_open_new_segment_*
mc_wal_segment_writer_total_chained_checksum_failures
...
```

---

## Class: `SegmentWriterUpdater`

The updater is the **hot‑path companion** that records metrics efficiently.

Example:

```cpp
updater.on_open_new_segment(start_ns, status);
updater.on_write_block_(start_ns);
updater.on_integrity_failure(status);
```

No locks. No branching beyond fast‑fail cases.

---

## Compile‑Time Invariants

The implementation enforces strict memory layout guarantees:

- Size must be a multiple of **64 bytes**
- Each metric block must begin on its own **cache line**
- Type must be **trivially copyable** and non‑movable

```cpp
static_assert(sizeof(SegmentWriter) % 64 == 0);
static_assert(alignof(SegmentWriter) == 64);
static_assert(std::is_trivially_copyable_v<SegmentWriter>);
```

---

## Summary

`SegmentWriter` telemetry is designed for:

- High‑frequency block writes  
- Predictable latencies  
- Monitoring integrity and performance  
- Avoiding hot‑path slowdown  

It forms a critical part of FlashStrike's **WAL durability, observability, and debugging ecosystem**.

---

## Related components

[`recorder::MetaStore`](./meta.md)
[`recorder::Manager`](./manager.md)

[`recorder::worker::SegmentPreparer`](./worker/segment_preparer.md)
[`recorder::worker::SegmentMaintainer`](./worker/segment_maintainer.md)

---
 
👉 Back to [`Telemetry – WAL Recorder Metrics Aggregator`](../telemetry.md)
