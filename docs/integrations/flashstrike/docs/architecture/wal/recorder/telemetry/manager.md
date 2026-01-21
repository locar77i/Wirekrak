# WAL Manager Telemetry – `telemetry::Manager`

This document describes the `flashstrike::wal::recorder::telemetry::Manager` structure and its associated `ManagerUpdater`.  
It is extracted from the provided C++ header and formatted as Markdown documentation.

---

## Overview

`telemetry::Manager` collects high‑resolution metrics for the WAL `Manager` subsystem.  
It tracks:

- Initialization time of the active segment  
- Append latency & histograms  
- Segment rotation latency  
- Background work‑planning timing  
- Persistence timing  
- Current hot/cold segment counts  

All fields are aligned to 64‑byte cache lines to avoid false sharing in highly concurrent systems.

---

## Full Structure

```cpp
struct alignas(64) Manager {
    alignas(64) stats::operation64 init_active_segment{};
    alignas(64) stats::operation64 append_event{};
    alignas(64) latency_histogram append_event_histogram{};
    alignas(64) stats::duration64 segment_rotation{};
    alignas(64) stats::duration64 work_planning{};
    alignas(64) stats::duration64 persist_current_segment{};
    alignas(64) gauge64 persistence_hot_segments{};
    gauge64 persistence_cold_segments{};
    char pad_[64 - (2 * sizeof(gauge64)) % 64] = {0};
};
```

### Key Metrics

| Metric | Description |
|-------|-------------|
| **init_active_segment** | Time to restore/create the first WAL segment |
| **append_event** | Latency of each append operation |
| **append_event_histogram** | Histogram of append latencies |
| **segment_rotation** | Duration of WAL segment rotation |
| **work_planning** | Time spent planning persistence work |
| **persist_current_segment** | Duration of flushing the active segment |
| **persistence_hot_segments** | Number of hot (uncompressed) segments |
| **persistence_cold_segments** | Number of cold (compressed) segments |

---

## Dump Format

`dump(label, os)` prints a human‑readable summary:

```
[Manager Metrics] Snapshot:
-----------------------------------------------------------------
 Init active segment   : <metric>
 Append event          : <metric>
 -> <percentiles>
 Rotation              : <metric>
 Work planning         : <metric>
 Persist current segm. : <metric>
 Current hot segments  : <value>
 Current cold segments : <value>
-----------------------------------------------------------------
```

---

## Collector Integration

All metrics are exportable:

```cpp
template <typename Collector>
void collect(const std::string& prefix, Collector& collector) const noexcept;
```

---

## ManagerUpdater

```cpp
class ManagerUpdater {
public:
    void on_init_active_segment(uint64_t start_ns, Status status) noexcept;
    void on_append_event(uint64_t start_ns, Status status) noexcept;
    void on_segment_rotation(uint64_t start_ns) noexcept;
    void on_work_planning(uint64_t start_ns) noexcept;
    void on_persist_current_segment(uint64_t start_ns) noexcept;
};
```

Each method records timing using a start timestamp and system monotonic clock.

---

## Compile‑Time Guarantees

The header enforces:

- `sizeof(Manager) % 64 == 0`
- 64‑byte alignment
- Every metric starts at a cache‑line boundary

This eliminates cross‑thread contention in high‑frequency metrics updates.

---

## Related components

[`recorder::MetaStore`](./meta.md)
[`recorder::SegmentWriter`](./segment_writer.md)

[`recorder::worker::SegmentPreparer`](./worker/segment_preparer.md)
[`recorder::worker::SegmentMaintainer`](./worker/segment_maintainer.md)

---
 
👉 Back to [`Telemetry – WAL Recorder Metrics Aggregator`](../telemetry.md)
