# WAL MetaState & MetaStore — Architecture Documentation

## MetaState (16‑byte WAL Progress Snapshot)

`MetaState` is the compact, trivially‑copyable structure used to represent the
current Write‑Ahead Log progress:

- **last_segment_index** — index of the most recent WAL segment  
- **last_offset** — byte offset within that segment  
- **last_event_id** — globally increasing event identifier  

### Key Properties
- Fixed size: **16 bytes**
- Trivially copyable
- No padding / dynamic data
- Safe for raw binary persistence (mmap, read/write)

```cpp
struct MetaState {
    uint32_t last_segment_index{0};
    uint32_t last_offset{0};
    uint64_t last_event_id{INVALID_EVENT_ID};
};
```

---

## MetaStore — Lock-Free Metadata Manager

`MetaStore` maintains and persists WAL progress while keeping the hot path free
of locks or syscalls.

### Responsibilities
- Track WAL state (`segment_index`, `offset`, `event_id`)
- Atomically update state during WAL appends
- Asynchronously persist metadata to disk
- Guarantee crash‑consistency using:
  - Temporary file writes  
  - `fdatasync()`  
  - Atomic filesystem `rename()`  

### Hot Path
`update()` performs:
- A single 64‑bit atomic store for `(segment_index, offset)`
- One atomic store for `last_event_id`
- A single atomic flag flip (`dirty_ = true`)

No locks, no syscalls.

### Persistence Path
`flush_to_disk()`:
1. Writes `state_` + `last_event_id_` to `<meta>.tmp`
2. Calls `fdatasync()`
3. Performs atomic `rename(tmp, meta)`
4. `fdatasync()` on the parent directory

Ensures atomic, crash‑safe metadata updates.

```cpp
class MetaStore {
public:
    explicit MetaStore(const std::string& dir, const std::string& fname, telemetry::MetaStore& metrics);

    void update(uint32_t last_segment_index, uint32_t last_offset, uint64_t last_event_id) noexcept;
    bool flush_to_disk() noexcept;
    bool load() noexcept;

    MetaState get_state() const noexcept;
    bool is_dirty() const noexcept;
    const std::string& filepath() const noexcept;

private:
    bool flush_to_disk_() noexcept;
};
```

---

## Usage Pattern

### Startup
```cpp
MetaStore meta(dir, "wal.meta", metrics);
if (!meta.load()) {
    // Initialize fresh metadata
}
```

### Hot Path
```cpp
meta.update(segment_index, offset, event_id);
```

### Background Thread
```cpp
if (meta.is_dirty()) {
    meta.flush_to_disk();
}
```

---

## Design Guarantees
- **Lock-free hot path:** Suitable for ultra‑low‑latency workloads (HFT engines, replication logs, telemetry streams)
- **Crash‑safe persistence:** Atomic rename ensures no partial writes
- **Predictable memory layout:** Enables zero‑overhead serialization

---

## File Format Layout (`wal.meta`)
Binary file (16 bytes):

```
[ 8 bytes packed: (segment_index << 32) | offset ]
[ 8 bytes last_event_id ]
```

Identical to the memory layout of `MetaState`.

---

## Summary

The WAL metadata subsystem is engineered for **nanosecond‑scale updates** and
**durable, crash‑safe persistence**, separating fast in‑memory state tracking from
asynchronous disk I/O. It ensures the WAL manager can always resume from the
last correct position after restart, crash, or segment rotation.

---

## Related components

[`recorder::Manager`](./manager.md)
[`recorder::SegmentWriter`](./segment_writer.md)
[`recorder::Telemetry`](./telemetry.md)

[`recorder::worker::MetaCoordinator`](./worker/meta_coordinator.md)
[`recorder::worker::SegmentPreparer`](./worker/segment_preparer.md)
[`recorder::worker::SegmentMaintainer`](./worker/segment_maintainer.md)

---

👉 Back to [`WAL Recorder System — Overview`](../recorder_overview.md)
