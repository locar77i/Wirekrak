# SegmentPreparer Telemetry (WAL Recorder)

This document describes the **`flashstrike::wal::recorder::telemetry::worker::SegmentPreparer`**  
metrics structure and its updater helper class.

---

## Overview

`SegmentPreparer` is the telemetry component that records performance metrics for the
**WAL Segment Preparer worker thread**, responsible for asynchronously preparing
new WAL segments ahead of time.

This telemetry module tracks timing data for operations performed by the Segment
Preparer, allowing introspection into system latency, bottlenecks, or backpressure
effects.

---

## Struct: `SegmentPreparer`

```cpp
struct alignas(64) SegmentPreparer {
    alignas(64) stats::duration64 get_next_segment{};
    ...
};
```

### Purpose

Tracks the latency of calls to:

- **`get_next_segment`** — time spent waiting for a pre‑prepared WAL segment to become available.

### Notes

- Entire struct is **64-byte aligned** to ensure each metric resides in its own
  cache line, avoiding false sharing in multi-threaded environments.
- The struct is non-copyable and non-movable to preserve metric integrity.

---

## Methods

### `copy_to(SegmentPreparer& other)`

Copies the telemetry snapshot (used when exporting or aggregating metrics).

### `dump(const std::string&, std::ostream&)`

Prints a human-readable snapshot:

```
[Segment Preparer Metrics] Snapshot:
-----------------------------------------------------------------
 Get next segment: <duration>
-----------------------------------------------------------------
```

### `collect(const std::string& prefix, Collector&)`

Exports metrics to a generic collector for monitoring systems.

Metrics exported:

| Metric | Description |
|--------|-------------|
| `<prefix>_get_next_segment_ns` | Time spent waiting to obtain the next prepared WAL segment |

---

## Updater Class

```cpp
class SegmentPreparerUpdater {
public:
    explicit SegmentPreparerUpdater(SegmentPreparer& metrics);
    void on_get_next_segment(uint64_t start_ns) const noexcept;
};
```

### Purpose

Records the elapsed time for a `get_next_segment()` operation by computing:

```
now_ns - start_ns
```

and storing the result in `metrics_.get_next_segment`.

---

## Compile-Time Guarantees

The implementation enforces:

```cpp
static_assert(sizeof(SegmentPreparer) % 64 == 0);
static_assert(alignof(SegmentPreparer) == 64);
static_assert(offsetof(SegmentPreparer, get_next_segment) % 64 == 0);
```

Ensuring:

- No cross-cache-line contamination
- Predictable memory layout
- Maximum throughput under heavy concurrency

---

## Summary

`SegmentPreparer` telemetry provides deep visibility into WAL segment preparation
latency—critical for diagnosing stalls, ensuring write‑ahead logging continuity,
and meeting ultra‑low‑latency exchange engine requirements.

---

## Related components

[`recorder::MetaStore`](../meta.md)
[`recorder::Manager`](../manager.md)
[`recorder::SegmentWriter`](../segment_writer.md)

[`recorder::worker::SegmentMaintainer`](./segment_maintainer.md)

---
 
👉 Back to [`Telemetry – WAL Recorder Metrics Aggregator`](../telemetry.md)
