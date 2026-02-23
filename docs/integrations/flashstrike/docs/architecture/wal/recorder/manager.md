# Flashstrike WAL Recorder — Manager (Detailed Documentation)

**File:** `flashstrike/wal/recorder/manager.hpp`  
**Generated doc:** `wal_manager.md`  
**Component:** `flashstrike::wal::recorder::Manager`  
**Purpose:** High-level coordinator for WAL writing, segment rotation, background persistence, compression, and metadata durability.

---

## Table of contents

1. [Overview](#overview)  
2. [Design Goals](#design-goals)  
3. [Key Responsibilities](#key-responsibilities)  
4. [Architecture & Components](#architecture--components)  
5. [Hot Path: append() — Behaviour & Guarantees](#hot-path-append----behaviour--guarantees)  
6. [Segment Lifecycle & Rotation](#segment-lifecycle--rotation)  
7. [Startup, Recovery & Initialization](#startup-recovery--initialization)  
8. [Background Workers & Interactions](#background-workers--interactions)  
9. [Data Structures & Resources](#data-structures--resources)  
10. [Durability, Checksums & Integrity](#durability-checksums--integrity)  
11. [Telemetry & Metrics](#telemetry--metrics)  
12. [API Summary & Examples](#api-summary--examples)  
13. [Operational Considerations](#operational-considerations)  
14. [Invariants & Sanity Checks](#invariants--sanity-checks)  
15. [Summary](#summary)

---

## Overview

`Manager` is the top-level WAL recorder component. Its role is to provide an **ultra-low-latency hot path** for appending `RequestEvent`s into a durable Write-Ahead Log while orchestrating asynchronous background tasks that prepare segments, persist and compress finished segments, and maintain durable metadata for recovery.

Critical design axiom: **no filesystem or blocking I/O on the append hot path.**

---

## Design Goals

- **Low latency:** appends must be allocation-free and avoid syscalls.
- **Durability:** data can be made durable with controlled syncs off the hot path.
- **Recoverability:** robust recovery from metadata or by scanning WAL segments.
- **Scalability:** support high event rates without stalling the matching engine.
- **Separation of concerns:** push long-lived I/O and CPU-heavy work to background workers.

---

## Key Responsibilities

- Maintain the active `SegmentWriter` used for immediate appends.
- Rotate WAL segments once they reach capacity, without blocking appenders.
- Use `SegmentPreparer` to have new segments ready (mmap'ed + prefaulted).
- Hand-off closed segments to `SegmentMaintainer` for `fsync`, compression, and retention.
- Keep `MetaState` updated and persisted via `MetaCoordinator`.
- On startup, scan existing files and attempt to restore last state.

---

## Architecture & Components

`Manager` composes the following pieces:

- **Active writer** (`std::unique_ptr<SegmentWriter>` or `std::shared_ptr`): the current segment open for append.
- **SegmentPreparer**: background producer of prepared `SegmentWriter`s.
- **SegmentMaintainer**: background consumer that closes, syncs, compresses, and deletes segments.
- **MetaCoordinator**: background flusher of `MetaState` (last segment index, offset, last event id).
- **Rings and buffers**:
  - `segments_to_persist_` (SPMC ring): ownership handoff of finished segments.
  - `segments_to_freeze_` (ring): filenames scheduled for compression.
  - `segments_to_free_` (ring): filenames scheduled for deletion.
  - `wal_files_`, `lz4_files_` (local ring buffers): short, in-process lists for planning.

All blocking or heavy tasks are executed by the background workers listed above.

---

## Hot Path: append() — Behaviour & Guarantees

### Purpose
`append(const RequestEvent& ev)` is the hot path used by the matching engine to persist incoming requests.

### Guarantees
- **Lock-free** and **allocation-free** (no std::vector/resizing/syscalls).
- No blocking I/O occurs.
- Updates an in-memory `MetaState` (atomic) for later flush.
- If the current segment fills, rotates to a prepared segment (prepared asynchronously).

### Steps performed (fast):
1. Assert active writer exists.
2. If `writer_->segment_is_full()`:
   - Call `rotate_segment_()` — hand off current writer and obtain next prepared segment.
3. Call `writer_->append(ev)` — appends into in-memory block.
4. Update `meta_state_.last_event_id` and `meta_state_.last_offset`.
5. Optionally record telemetry.

### Latency considerations
Append executes in nanoseconds to low hundreds of nanoseconds depending on CPU and branch prediction. The path avoids syscalls and memory allocation to keep latency minimal.

---

## Segment Lifecycle & Rotation

Segments move through the following states:

1. **Prepared** — created and mmap'ed by `SegmentPreparer` (ready for immediate use).
2. **Active** — currently receiving `append()` calls from the hot path.
3. **Written** — handed to the `segments_to_persist_` ring for durable close.
4. **Frozen/Compressed** — maintainer compresses hot `.wal` to `.lz4`.
5. **Deleted** — segments removed once retention policy dictates.

### Rotation algorithm (high-level)
- When active writer is full:
  1. Move active writer filepath into `wal_files_` for bookkeeping.
  2. Push ownership of `writer_` into `segments_to_persist_` (spin & relax if ring full).
  3. Get a prepared segment from `segment_preparer_.get_next_segment()` and set as new `writer_`.
  4. Update `meta_state_` to the new segment index and call `meta_coordinator_.update()`.

Rotation is designed to be non-blocking aside from best-effort spinning to enqueue the finished writer. The heavy I/O (fsync) happens in the maintainer.

---

## Startup, Recovery & Initialization

### initialize()
- Create WAL directory if missing.
- Call `scan_segments_()` to collect `.wal` and `.lz4` files on disk (sorted).
- Attempt to recover last state via `recover_last_state_()`:
  - Prefer `MetaCoordinator::load()` which reads small metadata file.
  - If meta file absent/corrupt, iterate `.wal` files from newest to oldest and attempt to read a valid `wal::segment::Header` using `read_segment_header()`.
- If no recoverable segment is found, create a new segment (`prepare_first_segment_from_scratch_()`).
- Start background workers:
  - `segment_preparer_.start(next_index)`
  - `maintainer_worker_.start()`
  - `meta_coordinator_.start()`

### Recovery notes
- Recovery is conservative: it tries the metadata file first, then falls back to scanning headers.
- On encountering corrupted segments, `pop_last_scanned_segment_()` attempts best-effort deletion to keep the directory clean.

---

## Background Workers & Interactions

### SegmentPreparer
- Preallocates and mmaps new `.wal` files.
- Prefaults pages (`touch()`) to avoid page-fault overhead on the hot path.
- Worker provides `get_next_segment()` to the Manager.

### SegmentMaintainer
- Receives finished `SegmentWriter` objects via `segments_to_persist_`.
- Calls `close_segment(sync=true)`, performs `fsync`, removes mmaps, and optionally compresses to `.lz4`.
- Enforces retention: pushes filenames to `segments_to_freeze_` and `segments_to_free_`.

### MetaCoordinator
- Receives `meta_state` updates from Manager.
- Flushes metadata to disk asynchronously using safe temp-file + rename + `fdatasync` semantics.
- Ensures `MetaState` durability for fast startup.

### Coordination patterns
- Manager -> Preparer: `get_next_segment()` (blocking wait, consumer).
- Manager -> Maintainer: push ownership of finished `SegmentWriter` to `segments_to_persist_`.
- Maintainer -> compression pipeline: push filenames into freeze/free rings.

Rings are sized to balance throughput vs memory footprint; the Manager uses spin-relax patterns when pushing into full rings.

---

## Data Structures & Resources

- **MetaState (fixed 16 bytes)**: `[uint32_t last_segment_index, uint32_t last_offset, uint64_t last_event_id]`.
- **SegmentWriter**: memory-mapped writer with in-memory active `Block`.
- **Rings**:
  - `segments_to_persist_` (SPMC) — ownership handoff for finished segments.
  - `segments_to_freeze_` (SPMC) — filenames to compress.
  - `segments_to_free_` (SPMC) — compressed filenames to delete.
- **Local ring buffers** (`lcr::local::ring`) for recent WAL/LZ4 files for planning.

---

## Durability, Checksums & Integrity

- Each `Block` has an XXH64 checksum and a chained checksum that includes the previous block, forming a forward integrity chain.
- `Header` also stores a checksum; `Header::verify()` checks header integrity and fields.
- `SegmentWriter::finalize_segment_header_()` sets `closed_ts_ns`, `last_chained_checksum`, and updates the header checksum and writes it (via `pwrite`).
- `SegmentMaintainer` calls `close_segment(sync=true)` to ensure kernel has persisted data; it may also explicitly `fsync` and `fdatasync` directory for durability.

---

## Telemetry & Metrics

If `ENABLE_FS1_METRICS` is defined, the Manager and workers report:

- Time to open/create segments.
- Segment write throughput.
- Time to rotate segments.
- Meta flush latencies.
- Compression success rates and sizes.
- Queue wait times when pushing to rings.

These metrics help identify I/O bottlenecks, tuning point for `num_blocks`, `max_segments`, and ring sizes.

---

## API Summary & Examples

### Construction
```cpp
Manager mgr("/var/lib/flashstrike/wal", num_blocks, max_segments, max_compressed_segments, metrics);
```

### Initialize
```cpp
Status s = mgr.initialize();
if (s != Status::OK) { /* handle error */ }
```

### Append (hot path)
```cpp
RequestEvent ev = ...;
Status s = mgr.append(ev); // lock-free, low-latency
```

### Get current meta state
```cpp
MetaState m = mgr.get_meta_state();
```

### Shutdown
```cpp
mgr.shutdown(); // flush current, stop workers
```

### Typical usage snippet
```cpp
Manager wal_mgr("/var/wal", 256, 32, 128, telemetry);
wal_mgr.initialize();

for (each incoming event) {
    wal_mgr.append(event);
}

wal_mgr.shutdown();
```

---

## Operational Considerations

- **Choosing `num_blocks`**: tradeoff between segment size and memory usage. Larger segments reduce rotation frequency but increase time to compress or fsync in background.
- **Retention limits (`max_segments`, `max_compressed_segments`)**: tune according to disk capacity and recovery window requirements.
- **Disk performance**: prefer low-latency NVMe for WAL directory.
- **Crash recovery**: ensure MetaStore metadata file is on the same device with proper directory `fdatasync` at flush time to guarantee atomic rename durability.
- **Monitoring**: watch metrics for ring full conditions and compression failures.

---

## Invariants & Sanity Checks

- `num_blocks_` ∈ [MIN_BLOCKS, MAX_BLOCKS].
- `segment_size_` == `sizeof(Header) + num_blocks_ * sizeof(Block)`.
- `meta_state_` always reflects last appended event id and writer offset.
- All pushed `SegmentWriter` instances are either fully valid or removed by maintainer on error.
- On shutdown, queues drained and metadata flushed.

---

## Summary

This Manager demonstrates production-grade systems engineering for ultra-low-latency infrastructure:

- Clear separation of hot and cold paths.
- Lock-free hot path with atomic metadata updates.
- Practical crash-consistency via checksums and atomic meta flush.
- Background workers handling expensive operations (mmap, fsync, compression).
- Tunable retention and performance knobs.
 
---

## Related components

[`recorder::SegmentWriter`](./segment_writer.md)
[`recorder::Meta`](./meta.md)
[`recorder::Telemetry`](./telemetry.md)

[`recorder::worker::MetaCoordinator`](./worker/meta_coordinator.md)
[`recorder::worker::SegmentPreparer`](./worker/segment_preparer.md)
[`recorder::worker::SegmentMaintainer`](./worker/segment_maintainer.md)

---

👉 Back to [`WAL Recorder System — Overview`](../recorder_overview.md)
