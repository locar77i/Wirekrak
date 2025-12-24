# FlashStrike WAL Recorder System — Architectural Overview

This document provides a high‑level overview of the **FlashStrike Write‑Ahead Log (WAL) Recorder System**, describing how its internal components collaborate to deliver a **lock‑free, ultra‑low‑latency, fault‑tolerant event logging pipeline** suitable for high‑frequency trading workloads.

---

## 1. System Goals

The WAL Recorder subsystem is engineered to:

- Provide **deterministic nanosecond‑scale append latency**.
- Guarantee **crash‑consistent durability semantics**.
- Support **continuous high‑throughput ingestion** without blocking.
- Separate hot‑path operations from disk I/O and retention logic.
- Allow **parallel segment preparation and maintenance** using background worker threads.
- Preserve a **compact and monotonic metadata state** for fast recovery.

---

## 2. Components Overview

| Component | Description |
|----------|-------------|
| [Manager](./recorder/manager.md) | High-level orchestrator of the WAL recorder (segment rotation, metadata, workers, retention, hot-path append). |
| [SegmentWriter](./recorder/segment_writer.md) | Writes events into WAL segments, manages headers/blocks, performs checksums and integrity validation. |
| [SegmentPreparer](./recorder/worker/segment_preparer.md) | Asynchronously pre-creates WAL segments so the hot path never blocks on I/O. |
| [SegmentMaintainer](./recorder/worker/segment_maintainer.md) | Background persistence, compression (LZ4), and retention enforcement for completed segments. |
| [MetaStore](./recorder/meta.md) | Lock-free atomic metadata store for segment index, offset, and last event ID. |
| [MetaCoordinator](./recorder/worker/meta_coordinator.md) | Background metadata flush worker ensuring durability (fdatasync + atomic rename). |
| [Telemetry](./recorder/telemetry.md) | Root telemetry container aggregating all recorder subsystem metrics. |

---

## 3. Main Components

### 3.1 Manager  
**Central orchestrator** of the entire WAL recorder subsystem.

Responsibilities:

- Maintains the **active WAL segment**.
- Performs **lock‑free append operations**.
- Coordinates:
  - **SegmentPreparer** for pre‑allocation.
  - **SegmentMaintainer** for persistence/compression/retention.
  - **MetaCoordinator** for metadata durability.
- Handles **crash recovery** and directory scanning at startup.

---

### 3.2 SegmentWriter  
Owns one WAL segment file and provides:

- Zero‑allocation, lock‑free `append()`.
- Fast checksum validation.
- Efficient block‑layout writing.
- Deterministic block rotation rules.

The **Manager** always interacts with exactly one active `SegmentWriter`.

---

### 3.3 SegmentPreparer (Background Worker)

Runs asynchronously.

- Pre‑allocates upcoming WAL segments.
- Ensures that segment rotation never blocks the hot path.
- Produces fully initialized `SegmentWriter` instances.
- Emits telemetry on segment preparation latency.

The Manager pulls pre‑created segments from this worker.

---

### 3.4 SegmentMaintainer (Background Worker)

Handles all **cold‑path operations**:

- Closing completed WAL segments.
- LZ4 compression of cooled segments.
- Old file retention and deletion.
- Directory load‑balancing / lifecycle metrics.

This worker guarantees that FST’s WAL remains **bounded in disk footprint** and efficiently compresses older data.

---

### 3.5 MetaStore + MetaCoordinator

The **metadata subsystem** provides:

- A compact **16‑byte MetaState** containing:
  - last segment index  
  - last byte offset  
  - last event ID  
- Lock‑free, atomic updates.
- Crash‑consistent fsync+rename persistence.
- Fast boot‑time recovery.

The Manager updates metadata on each append/rotation.

---

## 4. Hot Path vs. Cold Path Separation

### Hot Path (latency‑critical)
- `Manager::append()`
- `SegmentWriter::append()`
- Atomic updates to `MetaState`
- Zero system calls
- Zero dynamic allocations
- Cache‑aligned structures for deterministic timing

### Cold Path (background threads)
- Segment closing
- Compression (LZ4)
- Retention policies
- Metadata flushing
- Segment pre‑creation

This separation ensures **no I/O ever blocks the ingestion path**.

---

## 5. Segment Lifecycle Summary

1. **Active Segment**
   - Accepts append events.
   - Lives entirely in memory except for flushing blocks.

2. **Prepared Segment**
   - Pre‑created by SegmentPreparer.
   - Instantly swapped in on rotation.

3. **Completed Segment**
   - Finalized and passed to SegmentMaintainer.

4. **Compressed Segment**
   - Archived into `.lz4`.

5. **Cold Segment**
   - Retained until exceeding configured limits.

---

## 6. Startup & Recovery Model

At initialization:

1. Directory scanned for `.wal` and `.lz4` segments.
2. Metadata (`wal_meta.dat`) loaded.
3. If metadata missing → fallback recovery via segment headers.
4. Manager restores or creates the correct active segment.
5. Workers start:
   - Preparer (makes new segments)
   - Maintainer (compression/retention)
   - Metadata coordinator

Startup time remains extremely small due to fixed‑size metadata and shallow recovery procedures.

---

## 7. Telemetry Integration

All components expose metrics:

- Latency histograms
- Operation counters
- Lifecycle metrics
- Failure statistics

Telemetry is essential for HFT latency tuning, allowing observability of:

- Append latency percentiles
- Rotation timing
- Metadata flush cost
- Segment preparation delays
- Retention performance

---

## 8. Summary

The FlashStrike WAL Recorder is a **deterministic, fault‑tolerant, lock‑free logging pipeline** optimized for high‑frequency financial systems.

Key strengths:

- Zero‑blocking hot path
- Predictable latency
- Full crash recovery
- Scalable background pipeline
- Strong memory & I/O architecture discipline
- Highly instrumented via telemetry

It forms the foundational durability layer of the FlashStrike engine.

---

## 9. Related documents
 
`WAL Storage Architecture` — [Overview](./segment_overview.md)

`WAL Recovery System` — [Overview](./recovery_overview.md)

---

👉 Back to [`FlashStrike — High-Performance Matching Engine Documentation`](../../../index.md)
