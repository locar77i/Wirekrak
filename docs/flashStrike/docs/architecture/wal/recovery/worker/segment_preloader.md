# SegmentPreloader — WAL Recovery Background Loader

This document describes the **SegmentPreloader** component of the FlashStrike WAL
(Write-Ahead Log) recovery subsystem. The preloader is responsible for moving heavy
I/O and integrity‑verification work off the replay hot path, enabling deterministic,
low‑latency WAL recovery.

---

## Overview

`SegmentPreloader` is a background worker thread that performs:

1. **Asynchronous preloading of upcoming WAL segments**  
   Each segment is memory‑mapped, fully integrity‑verified, and wrapped in a
   `SegmentReader` instance before the main recovery thread needs it.

2. **Asynchronous closing of exhausted segments**  
   When the recovery manager finishes replaying a segment, it pushes the reader
   into a ring buffer. The preloader closes the file in the background, keeping the
   replay loop free of blocking syscalls.

This design removes heavy `mmap()`, checksum verification, and `close()` operations
from the recovery hot path.

---

## Key Responsibilities

### ✔ Preloading Future Segments  
The preloader receives a list of WAL files discovered during initial scanning.  
It sequentially:

- Opens each file  
- Memory‑maps it  
- Performs full integrity checks  
- Wraps it in a `SegmentReader`  
- Pushes it into the **prepared segments ring**

The recovery manager pops these ready-to-use readers with zero blocking.

---

### ✔ Asynchronous Segment Closure  
When a segment is consumed, the manager pushes the reader into the
**finished segments ring**.

The preloader:

- Pops finished readers  
- Calls `close_segment()`  
- Releases OS resources asynchronously  

This prevents replay latency spikes caused by file teardown.

---

## Runtime Model

```
 ┌─────────────────────────────┐
 │      Recovery Manager       │
 │  (hot replay of events)     │
 └───────────┬─────────────────┘
             │ pushes finished readers
             ▼
     finished_ring (SPSC)
             ▲
             │ pops finished readers
 ┌───────────┴─────────────────┐
 │      SegmentPreloader       │
 │  background thread          │
 │  • preload segments         │
 │  • close finished readers   │
 └───────────┬─────────────────┘
             │ pushes ready readers
             ▼
      prepared_ring (SPSC)
             ▲
             │ pops ready readers
 ┌───────────┴──────────────────┐
 │  Recovery Manager (continues)│
 └──────────────────────────────┘
```

---

## Performance Characteristics

- **Zero locking**: both rings are SPSC lock‑free structures  
- **Spin‑based wait strategy**: uses `cpu_relax()` and occasional yield  
- **Deterministic latency**: expensive I/O is fully removed from the replay loop  
- **Fast transitions**: segment switches cost microseconds instead of milliseconds  

Typical gains: **100–250 ms reduction** in worst-case segment transitions.

---

## Thread Lifecycle

### Starting:
```cpp
SegmentPreloader preloader(prepared, finished, metrics, reader_metrics);
preloader.start(segments);
```

### Stopping:
```cpp
preloader.stop();
```

Flags:

- `preloading_done_` → all future segments processed
- `done_` → worker fully exited

---

## Error Handling

- Corrupted or invalid segments are **skipped**, but never block replay.
- Failures in closing segments are logged but do not stop the worker.
- Metrics record:
  - preload success/failure  
  - segment close success/failure  

---

## Source API Summary

```
void start(std::vector<WalSegmentInfo> segments) noexcept;
void stop() noexcept;

bool preloading_is_done() const noexcept;
bool is_done() const noexcept;
```

Internal work loop:

```
• preload next WAL segment if ring has space
• close finished segments
• spin/yield if no work available
```

---

## Telemetry Integration

The component reports:

- Preload latency & success/failure  
- Segment close latency & success/failure  
- Preloader lifecycle timing  
- Per‑segment reader metrics (via SegmentReaderUpdater)  

---

## Related components

[`recovery::Manager`](./manager.md)
[`recovery::SegmentReader`](./segment_reader.md)
[`recovery::Telemetry`](./telemetry.md)

---

👉 Back to [`WAL Recovery System — Overview`](../../recovery_overview.md)

