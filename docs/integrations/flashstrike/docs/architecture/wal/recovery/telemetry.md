# WAL Recovery Telemetry Overview

This document provides a structured description of the `flashstrike::wal::recovery::Telemetry` class, which aggregates and manages all metric-collection components used during WAL (Write-Ahead Log) recovery operations.

---

## Overview

The WAL Recovery Telemetry system encapsulates three major metric subsystems:

- [`telemetry::SegmentReader`](./telemetry/segment_reader.md) — tracks latency, integrity verification, seek timing, and block-level statistics during WAL segment reading.
- [`telemetry::Manager`](./telemetry/manager.md) — captures high-level performance characteristics of the recovery manager, including replay throughput and event-handling latency.
- [`telemetry::worker::SegmentPreloader`](./telemetry/worker/segment_preloader.md) — monitors the background worker that preloads WAL segments and performs asynchronous closing of completed segments.

The `Telemetry` struct provides unified access to these metric modules and exposes helper methods for copying, serialization, and human-readable dumping.

---

## Telemetry Structure

```cpp
struct Telemetry {
    telemetry::SegmentReader segment_reader_metrics{};
    telemetry::worker::SegmentPreloader segment_preloader_metrics{};
    telemetry::Manager manager_metrics{};
};
```

### Key Responsibilities
- Owns all metric structures required by WAL recovery.
- Ensures metrics can be **copied**, **serialized**, and **dumped** consistently.
- Integrates with the global metrics collector.

---

## Copy Semantics

The `Telemetry` struct disables copy/move operations to prevent accidental sharing of underlying metric counters.  
Instead, it exposes a dedicated method:

```cpp
inline void copy_to(Telemetry& other) const noexcept;
```

This ensures that metric snapshots are transferred safely when required.

---

## Dumping Metrics

To print a human-readable snapshot:

```cpp
inline void dump(const std::string& label, std::ostream& os) const noexcept;
```

Example output:

```
-----------------------------------------------------------------
[Snapshot] WAL Recovery Telemetry:
-----------------------------------------------------------------
[Segment Reader Metrics]
...
[Segment Preloader Metrics]
...
[Manager Metrics]
...
```

This is only enabled when `ENABLE_FS1_METRICS` is compiled in.

---

## Metrics Collection

The unified collector API:

```cpp
template <typename Collector>
inline void collect(Collector& collector) const noexcept;
```

During collection:

- A metric namespace is pushed: `system = wal_recovery`
- Each subsystem exports its metrics under the prefix:

```
ie_wal_recovery_segment_reader_*
ie_wal_recovery_segment_preloader_*
ie_wal_recovery_manager_*
```

This allows easy ingestion into Prometheus, InfluxDB, or any structured monitoring backend.

---

## Integration Notes

The `Telemetry` struct is typically embedded inside:

- `flashstrike::wal::recovery::Manager`  
- Higher-level orchestration code for offline or online replay

It allows full observability into the recovery pipeline without interfering with hot-path behavior.

---

## File Purpose

This file provides the **documentation** for the telemetry aggregation layer of WAL recovery.  
For details on each subsystem, refer to:

- `segment_reader.md`
- `segment_preloader.md`
- `recovery_manager.md`

---

## Related components

[`recovery::Manager`](./manager.md)
[`recovery::SegmentReader`](./segment_reader.md)

[`recovery::worker::SegmentPreloader`](./worker/segment_preloader.md)

---

👉 Back to [`WAL Recovery System — Overview`](../recovery_overview.md)
