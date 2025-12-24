# WAL Recovery Subsystem — Overview

## Purpose

The **WAL Recovery subsystem** is responsible for reconstructing system state by replaying persisted Write‑Ahead Log (WAL) segments in strict sequential order.  
It is designed for **deterministic performance, ultra‑low latency**, and **lock‑free execution** in the hot path.

The recovery pipeline is composed of:

- **Manager** — orchestrates recovery, scanning segments, selecting the starting point, and streaming events.
- **SegmentReader** — performs mmap‑based segment access, full checksum validation, and high‑speed sequential reading.
- **worker::SegmentPreloader** — background engine that preloads and verifies segments ahead of the Manager.
- **Telemetry** — consolidated metrics aggregating Manager, Reader, and Worker performance.

---

## Components Overview

| Component | Description |
|----------|-------------|
| [Manager](./recovery/manager.md) | High-level orchestrator that scans WAL segments, resumes playback from a target event, and coordinates segment switching using the background preloader. |
| [SegmentReader](./recovery/segment_reader.md) | Low-level memory-mapped WAL segment reader with full checksum validation, sparse indexing, and sequential/event-ID seeking. |
| [SegmentPreloader](./recovery/worker/segment_preloader.md) | Background worker that preloads and verifies future WAL segments and asynchronously closes finished ones to keep the manager hot path I/O-free. |
| [Telemetry](./recovery/telemetry.md) | Root telemetry container aggregating all WAL recovery subsystem metrics. |

---

## Execution Flow

### 1. Initialization
1. Manager scans the WAL directory.
2. Segment headers are read and validated.
3. Segments are sorted by `first_event_id`.

### 2. Resume from event
Given an `event_id`, the Manager:

1. Locates the first segment whose range contains the event.
2. Opens and verifies that segment.
3. Starts the background **SegmentPreloader** with the remaining segments.
4. Seeks inside the segment to the correct event.

### 3. Event replay loop (`next()`)
The hot path:

1. Manager reads events through fast sequential `SegmentReader::next()`.
2. When the active segment is exhausted:
   - Pushes the reader to **finished_ring_**.
   - Pops a preloaded next segment from **prepared_ring_**.
3. Worker asynchronously:
   - Closes finished segments.
   - Preloads future ones.

---

## Component Responsibilities

### Manager
- Coordinates the full recovery lifecycle.
- Maintains current segment index.
- Ensures seamless transition between segments.
- Provides fallback synchronous open if the worker cannot keep up.
- Produces all recovery metrics (`ManagerUpdater`).

### SegmentReader
- mmap‑based zero‑copy segment access.
- Verifies:
  - header checksum  
  - block checksum  
  - chained checksum  
- Supports:
  - sequential reads  
  - sparse‑index assisted seeking  
- Exposes detailed integrity‑level telemetry.

### worker::SegmentPreloader
- Runs asynchronously in its own thread.
- Responsibilities:
  - Preloading upcoming segments.
  - Performing full verification.
  - Closing exhausted segments.
  - Maintaining smooth handoff into lock‑free rings.
- Guarantees that the Manager **never** performs slow I/O.

---

## Ring Buffers

Two **SPSC lock‑free** rings:

### prepared_ring_
- Writer = Preloader  
- Reader = Manager  
- Contains prevalidated segments.

### finished_ring_
- Writer = Manager  
- Reader = Preloader  
- Contains exhausted readers for asynchronous closure.

This lock‑free design ensures:
- Zero blocking  
- Predictable latency  
- Extremely fast transitions  

---

## Telemetry Structure

```
Telemetry
├── SegmentReader (open/close/verify/index/seek metrics)
├── worker::SegmentPreloader (preload/finish metrics)
└── Manager (seek, next-event latency, resume timing)
```

Every component:
- is cache‑aligned (64‑byte)
- avoids false sharing
- supports copy_to() for atomic snapshots
- supports structured collection for Prometheus‑style collectors

---

## Performance Profile

| Operation | Typical Cost |
|----------|--------------|
| Sequential read | 20–40 ns / event |
| Segment open + verify | 150–300 ms |
| Segment handoff (preloaded) | < 2 µs |
| Segment close async | Hidden from Manager hot path |
| Peak event throughput | 10–13M events/sec |

Because verification and closure occur in the background, the Manager’s runtime becomes almost purely computational.

---

## Failure Handling

- Corrupted segments are skipped by the worker.
- Manager only sees valid, ready‑to‑read segments.
- Proper shutdown ensures thread-safe stop + join.

---

## Summary

The WAL Recovery subsystem is optimized for:

- **Deterministic, ultra‑low‑latency replay**
- **Hidden I/O operations via background workers**
- **Lock‑free segment handoff**
- **Robust telemetry and diagnostics**
- **Safe corruption handling**

It provides a foundation for extremely high‑throughput, low‑variance recovery of event-driven systems.

## Related documents
 
`WAL Storage Architecture` — [Overview](./segment_overview.md)

`WAL Recorder System` — [Overview](./recorder_overview.md)

---

👉 Back to [`FlashStrike — High-Performance Matching Engine Documentation`](../../../index.md)
