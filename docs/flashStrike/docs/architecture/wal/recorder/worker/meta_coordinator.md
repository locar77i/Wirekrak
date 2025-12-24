# WAL MetaCoordinator — Architecture Documentation

## Overview
The **MetaCoordinator** is the asynchronous metadata persistence manager for the FlashStrike WAL subsystem.  
It wraps the low-level `MetaStore` (which handles atomic hot‑path metadata updates) and adds a dedicated
background thread responsible for flushing metadata safely to disk without impacting latency.

This document explains the design, responsibilities, performance considerations, and lifecycle
of `MetaCoordinator`.

---

## Purpose

The `MetaCoordinator` enables:

- **Lock‑free metadata updates** from the hot path (WAL append thread).
- **Asynchronous, crash‑consistent persistence** of WAL metadata to disk.
- **Efficient signaling** via a condition variable to avoid CPU waste.
- **Recovery support**: persisted metadata is automatically loaded at startup.

It ensures the WAL subsystem always knows:

- The last written WAL segment index  
- The byte offset inside the last segment  
- The globally increasing last event ID  

These fields together represent the WAL's durable replay position.

---

## Responsibilities

### ✔ Accept Hot‑Path Metadata Updates
`update(const MetaState&)` pushes new metadata values to the underlying `MetaStore`
using its lock-free atomic mechanism.

### ✔ Wake Background Flusher
Whenever metadata is updated, the coordinator signals the background thread to persist it.

### ✔ Flush Dirty Metadata to Disk Asynchronously
The background thread (`flush_loop_()`) wakes up only when:

- New metadata is dirty  
- The system is shutting down  

It invokes:

```
meta_store_.flush_to_disk()
```

This ensures crash-consistent persistence using:

- Temporary file + atomic rename  
- `fdatasync` for durability

### ✔ Startup Recovery
On startup:

```
meta_coordinator.load()
```

restores WAL metadata from the previous run.

### ✔ Thread Lifecycle Management
- `start()` launches the flush thread  
- `stop()` safely terminates it and flushes outstanding metadata  

---

## Thread Safety Model

### **Hot Path** (lock-free)
- `update()`
- `get_state()`

These can be called concurrently without blocking.

### **Background Thread** (single-threaded serialization)
- only performs disk flush operations
- no locking interactions with WAL writers

### **Mutex / Condition Variable**
Used only for wake-up coordination—not for protecting shared data.

This minimizes latency and avoids contention.

---

## Internal Architecture

```
 ┌─────────────────────┐        update() (lock‑free)
 │  WAL Append Thread  │ ───────────────────────────────┐
 └─────────────────────┘                                │
                                                         ▼
                                               ┌────────────────┐
                                               │   MetaStore    │
                                               │  (atomic WAL   │
                                               │    position)   │
                                               └────────────────┘
                                                         │
                               background thread wakes   │ is_dirty()
                               via condition_variable    ▼
                                               ┌────────────────┐
                                               │ flush_loop_()  │
                                               │  async fsync   │
                                               └────────────────┘
                                                         │
                                                         ▼
                                           Persistent WAL metadata file
```

---

## Lifecycle

### Initialization
```cpp
MetaCoordinator meta(dir, "wal.meta", metrics);
meta.load();
meta.start();
```

### Hot‑Path Update
```cpp
meta.update(state);
```

### Read Metadata
```cpp
MetaState s = meta.get_state();
```

### Shutdown
```cpp
meta.stop();
```

---

## Performance Characteristics

- **Zero-cost hot path:** No syscalls, no locks, no dynamic memory.
- **Flush thread sleeps efficiently** until signaled.
- **Condition variable avoids busy loops**.
- **Disk flushing is offloaded** from trading-critical code.

Designed to support **ultra‑low‑latency matching engine workloads**.

---

## Metadata Persistence Guarantees

On every flush:

1. Write metadata to a temporary file  
2. `fdatasync()` temporary file  
3. Atomic rename to final metadata file  
4. `fdatasync()` directory  

This ensures:

- no partial metadata writes  
- no corruption even on power loss  
- metadata is always recoverable  

---

## File Layout (16 bytes)

| Field               | Size | Description |
|--------------------|------|-------------|
| last_segment_index | 4    | WAL segment index |
| last_offset        | 4    | offset within .wal file |
| last_event_id      | 8    | global ordering anchor |

---

## Conclusion

The `MetaCoordinator` is a crucial part of FlashStrike’s durability pipeline.  
It ensures that metadata is always:

- **atomic**
- **lock-free on the hot path**
- **persisted asynchronously**
- **crash-safe**
- **restart‑recoverable**

Its design enables consistent WAL replay and safe continuation after crashes without
impacting trading performance.

---

## Related components

[`recorder::Manager`](../manager.md)
[`recorder::SegmentWriter`](../segment_writer.md)
[`recorder::Meta`](../meta.md)
[`recorder::Telemetry`](../telemetry.md)

[`recorder::worker::SegmentPreparer`](./segment_preparer.md)
[`recorder::worker::SegmentMaintainer`](./segment_maintainer.md)

---

👉 Back to [`WAL Recorder System — Overview`](../../recorder_overview.md)
