# SegmentPreloader Telemetry (WAL Recovery Worker)

This document describes the telemetry structure **SegmentPreloader** and its companion
**SegmentPreloaderUpdater**, used inside the Flashstrike WAL Recovery subsystem to collect
performance metrics for the background segment–preloading worker.

## Overview

The preloader worker performs two main asynchronous tasks:

1. **Preloading WAL segments** ahead of the recovery manager.
2. **Closing finished segments** asynchronously to keep the hot path free of I/O.

To measure these operations with high precision, the `SegmentPreloader` struct exposes two
metrics:

- `preload_segment` — time required to open + verify a WAL segment.
- `finish_segment` — time required to close an exhausted WAL segment.

All fields are 64‑byte aligned to avoid false sharing and ensure deterministic latency.

## Telemetry Structure

```cpp
struct alignas(64) SegmentPreloader {
    alignas(64) stats::operation64 preload_segment{};
    alignas(64) stats::operation64 finish_segment{};
    ...
};
```

### Metrics Recorded

| Metric | Description |
|--------|-------------|
| **preload_segment** | Measures the full cost of preloading (mmap + full verification chain). |
| **finish_segment** | Measures the time spent asynchronously closing segments. |

Both metrics support success/failure recording and duration measurement.

## Human‑Readable Dump

`dump(label, os)` prints a structured snapshot:

```
[label Metrics] Snapshot:
-----------------------------------------------------------------
 Preload segment: X ms
 Finish segment : Y ms
-----------------------------------------------------------------
```

## Collector Integration

```cpp
collector.push_label("subsystem", "wal_recovery_worker");
preload_segment.collect(prefix + "_preload_segment", collector);
finish_segment.collect(prefix + "_finish_segment", collector);
collector.pop_label();
```

This allows Prometheus‑style hierarchical serialization.

## Updater Class

The `SegmentPreloaderUpdater` provides zero‑overhead metric updates:

```cpp
inline void on_preload_segment(uint64_t start_ns, Status status);
inline void on_finish_segment(uint64_t start_ns, Status status);
```

Usage pattern:

```cpp
auto start = now_ns();
Status st = preload();
metrics.on_preload_segment(start, st);
```

## Compile‑Time Guarantees

The struct enforces:

- 64‑byte alignment
- 64‑byte size granularity
- cache‑line alignment for every metric

```cpp
static_assert(sizeof(SegmentPreloader) % 64 == 0);
static_assert(alignof(SegmentPreloader) == 64);
```

This protects the worker thread from false sharing during tight spin‑loops.

## Summary

`SegmentPreloader` telemetry provides:

- Highly accurate I/O and verification timing
- Cache‑line–isolated metric recording
- Seamless integration with the recovery manager + Prometheus collectors
- Zero impact on the hot path of WAL replay

It is a critical component for performance analysis and tuning of the Flashstrike recovery pipeline.

---

## Related components

[`recovery::telemetry::Manager`](../manager.md)
[`recovery::telemetry::SegmentReader`](../segment_reader.md)

---
 
👉 Back to [`Telemetry – WAL Recovery Metrics Aggregator`](../../telemetry.md)
