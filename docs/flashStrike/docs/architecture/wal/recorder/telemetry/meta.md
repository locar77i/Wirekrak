# MetaStore Telemetry Documentation

## Overview
This document provides a detailed explanation of the **MetaStore telemetry subsystem** used in the FlashStrike WAL recorder.

---

## Struct: `MetaStore`

### Description
`MetaStore` collects metrics related to **metadata flush operations** performed by the WAL subsystem.  
It is aligned to 64 bytes to prevent false sharing and ensure cache efficiency.

### Fields
- `maintenance_meta_flush` — duration of metadata flush operations.

### Characteristics
- Cache-line aligned (`alignas(64)`).
- Not copyable or movable.
- Designed for extremely fast, low-overhead metric recording.

```cpp
struct alignas(64) MetaStore {
    alignas(64) stats::duration64 maintenance_meta_flush{};
    ...
};
```

---

## Methods

### `copy_to(MetaStore& other)`
Copies metric values into another instance.

### `dump(const std::string& label, std::ostream& os)`
Renders a human‑readable summary of the telemetry snapshot.

### `collect(const std::string& prefix, Collector& collector)`
Exports metrics to an external collector, such as Prometheus-compatible systems.

---

## Compile-Time Guarantees
```cpp
static_assert(sizeof(MetaStore) % 64 == 0);
static_assert(alignof(MetaStore) == 64);
static_assert(offsetof(MetaStore, maintenance_meta_flush) % 64 == 0);
```

---

## Class: `MetaUpdater`

### Description
Helper responsible for updating the associated `MetaStore` telemetry.

### Method
#### `on_async_meta_flush_completed(uint64_t start_ns)`
Records the duration of a metadata flush operation using a high‑precision monotonic clock.

```cpp
inline void on_async_meta_flush_completed(uint64_t start_ns) const noexcept {
    metrics_.maintenance_meta_flush.record(start_ns, monotonic_clock::instance().now_ns());
}
```

---

## Summary
The **MetaStore telemetry module** provides:
- Ultra‑lightweight flush‑latency tracking  
- Cache‑aligned, false‑sharing‑free metrics  
- Clean integration with metric collectors  
- Strong invariants guaranteeing performance and safety

This component is a foundational layer of the WAL recorder’s observability and performance diagnostics.

---

## Related components

[`recorder::Manager`](./manager.md)
[`recorder::SegmentWriter`](./segment_writer.md)

[`recorder::worker::SegmentPreparer`](./worker/segment_preparer.md)
[`recorder::worker::SegmentMaintainer`](./worker/segment_maintainer.md)

---
 
👉 Back to [`Telemetry – WAL Recorder Metrics Aggregator`](../telemetry.md)
