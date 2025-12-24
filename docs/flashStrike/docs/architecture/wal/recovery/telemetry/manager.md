# WAL Recovery Manager Telemetry

This document describes the telemetry structure **Manager** and its companion
**ManagerUpdater**, used inside the FlashStrike WAL Recovery subsystem to collect
performance metrics for the high-level recovery orchestrator.

## Overview
The **Manager telemetry structure** captures performance and correctness‑related metrics
for the WAL Recovery Manager. These metrics quantify the cost of scanning, seeking,
resuming recovery, and iterating events during WAL replay.

The system guarantees **cache‑line alignment (64 bytes)** to avoid false sharing and
ensure predictable performance.

---

## Tracked Metrics

### `read_segment_header`
Measures the time required to parse and validate each WAL segment header during directory scanning.

### `resume_from_event`
Captures the latency of resuming WAL playback from a specific `event_id`,
including:
- locating the correct WAL segment,
- opening it,
- performing integrity checks,
- and seeking to the target event offset.

### `seek_event`
Direct duration measurement of the internal `reader_->seek(event_id)` operation.

### `next_event`
Measures latency of retrieving the next event from the current WAL segment.
Includes full replay path.

### `next_event_histogram`
A high‑resolution latency histogram recording nanosecond‑level statistics for the
hot‑path replay, enabling _p50/p90/p99/p99.9_ performance tracking.

---

## Dump Format

Calling `dump(label, os)` produces:

```
[<label> Metrics] Snapshot:
-----------------------------------------------------------------
 Read segment header: <duration>
 Resume from event  : <duration>
 Seek event         : <duration>
 Next event         : <duration>
 -> <percentile histogram>
-----------------------------------------------------------------
```

---

## Metric Collection API

The `collect(prefix, collector)` method serializes all metrics into an external
collector (Prometheus, CSV, internal pipeline, etc.) with the following keys:

```
<prefix>_read_segment_header
<prefix>_resume_from_event
<prefix>_seek_event
<prefix>_next_event
<prefix>_next_event_histogram
```

---

## ManagerUpdater

`ManagerUpdater` provides zero‑overhead hooks used by the recovery engine:

| Callback | Purpose |
|---------|---------|
| `on_read_segment_header(start_ns, status)` | Records header parse time |
| `on_resume_from_event(start_ns, status)`   | Tracks resume/restart cost |
| `on_seek_event(start_ns)`                  | Measures seek latency |
| `on_next_event(start_ns)`                  | Records per‑event and histogram timings |

All callbacks rely exclusively on `monotonic_clock::now_ns()` for nanosecond precision.

---

## Compile-Time Safety

The following invariants are enforced:

- Struct size is a **multiple of 64 bytes**
- Alignment is **64 bytes**
- Each major metric begins at a **cache‑line boundary**

This ensures:
- zero false sharing,
- predictable SMP behavior,
- optimal layout for high‑frequency metric updates.

---

## Summary

`telemetry::Manager` provides **high‑fidelity**, **low‑overhead**, and
**HFT‑grade** introspection into the WAL recovery process, enabling precise
performance tuning and post‑mortem debugging.

---

## Related components

[`recovery::telemetry::SegmentReader`](./segment_reader.md)

[`recovery::telemetry::worker::SegmentPreloader`](./worker/segment_preloader.md)

---
 
👉 Back to [`Telemetry – WAL Recovery Metrics Aggregator`](../telemetry.md)
