# WAL Segment Preparer — Asynchronous Segment Initialization Worker

## Overview
The `SegmentPreparer` is a background worker responsible for asynchronously creating and initializing new WAL segments so that the main write-ahead logging hot path never blocks on filesystem or memory-mapping operations.

It ensures:
- New segments are fully allocated and memory-mapped ahead of time  
- Pages are prefaulted to avoid latency spikes  
- Segment headers are initialized and ready for event append  
- A queue of prepared `SegmentWriter` instances is always available  

This document summarizes architecture, threading model, invariants, and interaction with the WAL subsystem.

---

## Purpose
High-frequency trading workloads require **zero-stall persistence**. Any filesystem operation—file creation, mmap, ftruncate, page fault—can introduce latency spikes of **tens to hundreds of microseconds**.

`SegmentPreparer` moves this work **off the critical path** by preparing future WAL segments asynchronously.

---

## Responsibilities
- Maintain a queue of preallocated WAL segments  
- Create new segment files in a background thread  
- Memory-map segment files  
- Prefault pages  
- Initialize headers  
- Provide segments to WAL manager on demand  
- Apply backpressure when queue is full  
- Report telemetry metrics  

---

## Queueing & Backpressure
Uses a lockfree SPSC ring buffer:

```
PREPARE_QUEUE_CAPACITY = 4
```

This small capacity is intentional:
- Limits memory usage  
- Prevents runaway preallocation  
- Ensures segments are fresh  
- Applies natural backpressure  

When queue is full:
- Worker yields (`std::this_thread::yield()`)

When queue is empty:
- Consumer waits via condition variable

---

## Threading Model

### Background Worker Thread
Started via:

```cpp
preparer.start(initial_segment_index);
```

Stopped via:

```cpp
preparer.stop();
```

Thread safely:
- Worker thread → pushes prepared segments  
- Main thread → pops prepared segments  

Only safe consumer API:

```cpp
auto seg = preparer.get_next_segment();
```

---

## Segment Preparation Lifecycle
Inside the worker loop:

1. Generate a unique WAL filename  
2. Allocate a new `SegmentWriter`  
3. Call `open_new_segment()`  
4. Prefault pages via `touch()`  
5. Push to the SPSC queue  
6. Notify consumer  

Segments retrieved via `get_next_segment()` are **ready for immediate event append**.

---

## Interaction With SegmentWriter
A prepared segment includes:
- Preallocated file (ftruncate)  
- Memory mapped pages (mmap)  
- Prefaulted memory (touch)  
- Written segment header  
- Ready for `append(event)` with no setup delay  

This completely eliminates file I/O latency from the hot path.

---

## Shutdown Behavior
When `stop()` is called:

- Background thread exits  
- Condition variables are signaled  
- Queue is drained  
- Any unused segments are destroyed  
- No file descriptors or mmap regions leak  

---

## Metrics Integration
`SegmentPreparer` integrates with telemetry:

- Time to create segments  
- Time consumer waits for segment availability  
- SegmentWriter initialization latency  

This helps profile WAL performance under production load.

---

## API Summary

```cpp
SegmentPreparer(const std::string& dir,
                size_t num_blocks,
                telemetry::worker::SegmentPreparer& metrics,
                telemetry::SegmentWriter& segment_metrics);

void start(size_t initial_segment_index);
void stop();

std::shared_ptr<SegmentWriter> get_next_segment();
```

---

## Invariants
- Queue size never exceeds `PREPARE_QUEUE_CAPACITY`  
- Each prepared segment has a unique index  
- All returned segments are ready for immediate append  
- Worker thread is single-producer, consumer is single-consumer  

---

## Related components

[`recorder::Manager`](../manager.md)
[`recorder::SegmentWriter`](../segment_writer.md)
[`recorder::Meta`](../meta.md)
[`recorder::Telemetry`](../telemetry.md)

[`recorder::worker::MetaCoordinator`](./meta_coordinator.md)
[`recorder::worker::SegmentMaintainer`](./segment_maintainer.md)

---

👉 Back to [`WAL Recorder System — Overview`](../../recorder_overview.md)
