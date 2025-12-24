# WAL Segment Maintainer Telemetry
Generated from the C++ header definition of `SegmentMaintainer` and `SegmentMaintainerUpdater`.

## Overview
This module provides high-resolution performance and correctness telemetry for the **SegmentMaintainer** worker, responsible for WAL persistence, retention, compression, and cold-segment deletion.

It is designed for ultra-low-latency systems and uses:
- cache-line–aligned metric groups,
- efficient `stats::operation64`, `stats::life_cycle`, and gauge structures,
- explicit histograms for performance tracking,
- structured collectors for downstream monitoring pipelines.

---

## Struct: `SegmentMaintainer`
```cpp
struct alignas(64) SegmentMaintainer {
    alignas(64) stats::life_cycle persistence_lifecycle{};
    alignas(64) stats::operation64 maintenance_retention{};
    alignas(64) stats::operation64 maintenance_compression{};
    alignas(64) stats::operation64 maintenance_deletion{};
    alignas(64) constant_gauge_u64 persistence_max_hot_segments{0};
    constant_gauge_u64 persistence_max_cold_segments{0};
    char pad_[64 - (2 * sizeof(constant_gauge_u64)) % 64] = {0};
};
```
### What It Tracks
- **persistence_lifecycle** — timing of the worker's persistence loop, including active and idle phases.
- **maintenance_retention** — time + success state of hot segment retention decisions.
- **maintenance_compression** — performance + success state of individual segment compression jobs.
- **maintenance_deletion** — metrics for deletion of cold (compressed) segments.
- **persistence_max_hot_segments** — configured maximum number of hot segments.
- **persistence_max_cold_segments** — configured maximum number of cold segments.

---

## Methods: Copy, Dump, Collect

### `copy_to()`
Allows copying metrics safely without violating alignment or atomic constraints.

### `dump()`
Human-readable diagnostic output used in debug and live environments.

### `collect()`
Serializes the metric set into a generic collector (Prometheus, OpenTelemetry, custom exporters, etc.).

---

## Class: `SegmentMaintainerUpdater`
```cpp
class SegmentMaintainerUpdater {
public:
    explicit SegmentMaintainerUpdater(SegmentMaintainer& metrics);

    void on_persistence_loop(bool did_work, uint64_t start_ns, uint64_t sleep_time) const noexcept;
    void on_hot_segment_retention(bool ok, uint64_t start_ns) const noexcept;
    void on_hot_segment_compression(bool ok, uint64_t start_ns) const noexcept;
    void on_cold_segment_deletion(bool ok, uint64_t start_ns) const noexcept;

    void set_max_segments(size_t max_segments) const noexcept;
    void set_max_compressed_segments(size_t max_compressed_segments) const noexcept;
};
```

### What the Updater Does
- Instruments **every iteration** of the maintain loop.
- Measures cost of:
  - retention checks,
  - compression jobs,
  - deletion operations.
- Records whether work was performed or the loop slept idle.
- Updates gauges for segment limits dynamically at runtime.

---

## Compile-Time Checks
The implementation enforces critical invariants:

```
sizeof(SegmentMaintainer) % 64 == 0
alignof(SegmentMaintainer) == 64
offsetof(... each metric ...) % 64 == 0
```

These ensure:
- no metric shares a cache line with another,
- no false sharing occurs between threads,
- predictable memory layout for high-performance telemetry.

---

## Summary
This telemetry module is essential for diagnosing WAL durability, retention efficiency, and segment housekeeping performance.

It is designed for HFT-grade workloads: predictable, lock-free, and introspectable.

---

## Related components

[`recorder::MetaStore`](../meta.md)
[`recorder::Manager`](../manager.md)
[`recorder::SegmentWriter`](../segment_writer.md)

[`recorder::worker::SegmentPreparer`](./segment_preparer.md)

---
 
👉 Back to [`Telemetry – WAL Recorder Metrics Aggregator`](../telemetry.md)
