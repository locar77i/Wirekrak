# `Telemetry` – WAL Recorder Metrics Aggregator

This document describes the **`flashstrike::wal::recorder::Telemetry`** struct, which serves as a unified container for all WAL recorder–related metric subsystems.

---

## Overview

`Telemetry` groups together five independent metric subsystems:

- **MetaStore metrics** — durability and metadata persistence.
- **SegmentWriter metrics** — WAL append throughput, block finalization latency, etc.
- **SegmentPreparer metrics** — async preallocation throughput and timing.
- **SegmentMaintainer metrics** — persistence, compression, retention.
- **Manager metrics** — high‑level orchestration, rotations, hot‑path timing.

This struct is **non-copyable** and **non-movable**, ensuring that metric ownership remains unique within the WAL recorder subsystem.

---

## Structure Definition

```cpp
struct Telemetry {
    telemetry::MetaStore meta_store_metrics{};
    telemetry::SegmentWriter segment_writer_metrics{};
    telemetry::worker::SegmentPreparer segment_preparer_metrics{};
    telemetry::worker::SegmentMaintainer segment_maintainer_metrics{};
    telemetry::Manager manager_metrics{};

    Telemetry() = default;
    Telemetry(const Telemetry&) = delete;
    Telemetry& operator=(const Telemetry&) = delete;
    Telemetry(Telemetry&&) noexcept = delete;
    Telemetry& operator=(Telemetry&&) noexcept = delete;

    inline void copy_to(Telemetry& other) const noexcept;

    inline void dump(const std::string& label, std::ostream& os) const noexcept;

    template <typename Collector>
    inline void collect(Collector& collector) const noexcept;
};
```

---

## Key Responsibilities

### ✅ Centralized Metric Storage
The struct aggregates all WAL recorder metric components in one place, simplifying:

- lifecycle management  
- reporting  
- serialization to external monitoring systems  

### ✅ Controlled Copy Semantics
The struct explicitly **disables**:

- copy constructor  
- copy assignment  
- move constructor  
- move assignment  

This ensures metrics are **not accidentally duplicated**, preventing double-counting or inconsistent reporting.

A dedicated `copy_to()` method performs deterministic metric copying when explicitly required.

---

## `copy_to()` Method

Copies each metric subsystem into another `Telemetry` instance:

```cpp
inline void copy_to(Telemetry& other) const noexcept {
    meta_store_metrics.copy_to(other.meta_store_metrics);
    segment_writer_metrics.copy_to(other.segment_writer_metrics);
    segment_preparer_metrics.copy_to(other.segment_preparer_metrics);
    segment_maintainer_metrics.copy_to(other.segment_maintainer_metrics);
    manager_metrics.copy_to(other.manager_metrics);
}
```

Use case: snapshotting metrics or publishing them to external monitoring components.

---

## Dumping Metrics

`dump()` prints human-readable metrics if `ENABLE_FS1_METRICS` is enabled:

```cpp
inline void dump(const std::string& label, std::ostream& os) const noexcept {
    os << "-----------------------------------------------------------------
";
    os << "[" << label << "] WAL Recorder Metrics:
";
    os << "-----------------------------------------------------------------
";
#ifdef ENABLE_FS1_METRICS
    meta_store_metrics.dump("Meta Store", os);
    segment_writer_metrics.dump("Segment Writer", os);
    segment_preparer_metrics.dump("Segment Preparer", os);
    segment_maintainer_metrics.dump("Segment Maintainer", os);
    manager_metrics.dump("Manager", os);
#endif
}
```

This is useful for:

- debugging
- profiling
- CLI diagnostic tools

---

## Serialization (`collect()`)

All metric subsystems expose a uniform serialization interface used for pushing metrics into external collectors:

```cpp
collector.push_label("system", "wal_recorder");
// ...
collector.pop_label();
```

Metrics are emitted under the prefix:

```
ie_walrecorder_*
```

Examples:

- `ie_wal_recorder_meta_store_*`
- `ie_wal_recorder_segment_writer_*`
- `ie_wal_recorder_segment_preparer_*`
- `ie_wal_recorder_segment_maintainer_*`
- `ie_wal_recorder_manager_*`

---

## Subcomponents

`Telemetry` is the **central metrics hub** of the WAL recorder:

| Subcomponent | Purpose |
|----------|---------|
| [`telemetry::Manager`](./telemetry/manager.md) | High‑level orchestration metrics |
| [`telemetry::SegmentWriter`](./telemetry/segment_writer.md) | WAL append performance |
| [`telemetry::MetaStore`](./telemetry/meta.md) | Metadata persistence & durability metrics|
| [`telemetry::worker::SegmentPreparer`](./telemetry/worker/segment_preparer.md) | Async preallocation metrics |
| [`telemetry::worker::SegmentMaintainer`](./telemetry/worker/segment_maintainer.md) | Persistence, compression and retention metrics|

It ensures **consistent, safe, structured** metrics reporting across all WAL components.

---
 
## Related components

[`recorder::Manager`](./manager.md)
[`recorder::SegmentWriter`](./segment_writer.md)
[`recorder::Meta`](./meta.md)

[`recorder::worker::MetaCoordinator`](./worker/meta_coordinator.md)
[`recorder::worker::SegmentPreparer`](./worker/segment_preparer.md)
[`recorder::worker::SegmentMaintainer`](./worker/segment_maintainer.md)

---

👉 Back to [`WAL Recorder System — Overview`](../recorder_overview.md)
