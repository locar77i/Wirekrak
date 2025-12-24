# WAL Recovery System — Manager & Preloader Overview

## Overview

The WAL (Write-Ahead Log) **Recovery Subsystem** reconstructs system state by replaying persisted WAL
segments in strict order. It ensures *deterministic*, *low-latency*, and *lock‑free* replay behavior.

This document describes the architecture and responsibilities of the following recovery components:

- **Manager** — orchestrates recovery and event replay
- **SegmentPreloader (worker)** — asynchronously preloads upcoming segments and closes exhausted ones
- **Lock-free rings** used for high‑speed handoff
- **Telemetry** integration
- **Diagnostic reader/manager** baseline architecture

---

## Architectural Goals

The WAL recovery design targets:

### Deterministic replay
Replay must behave identically across executions, independent of storage timing or CPU scheduling.

### Ultra‑low event latency
Event replay must remain below 50–100 ns per event, even during segment transitions.

### Zero I/O on the hot path
All expensive operations—mmap, integrity checks, segment closure—are performed **asynchronously** in the background.

### Strict recovery semantics
The system must:
- Validate *every block* (checksum + chained checksum)
- Reject invalid segments
- Abort recovery on corruption (strict mode)

### High throughput
Target: **10–15 million events/sec** on modern NVMe SSD systems.

---

## Components

### `Manager`
The high‑level orchestrator responsible for:

- Scanning all `.wal` files on disk  
- Building `WalSegmentInfo` structures  
- Finding the correct segment for a given `event_id`  
- Driving the replay loop (`next()`)  
- Maintaining zero‑stall transitions between segments  
- Sending exhausted segments to background cleanup  
- Integrating recovery telemetry

**Manager owns the active `SegmentReader`.**

When it reaches the end of the current segment:

1. Pushes the exhausted reader into `finished_ring_`
2. Pops a **preloaded** next reader from `prepared_ring_`
3. If none exist, falls back to synchronous open (rare path)

This architecture ensures that segment transitions are **microsecond‑level** operations.

---

### `worker::SegmentPreloader`

Background worker thread performing two jobs:

#### **1. Preloading future segments**
- Opens segment file (`mmap`)
- Verifies integrity (header + every block)
- Builds sparse index
- Pushes fully validated reader into `prepared_ring_`

This removes **200–400 ms** of latency ordinarily required to open + verify a segment.

#### **2. Closing finished segments**
Any exhausted segment is pushed into `finished_ring_` by the Manager.

The worker:
- Pops from `finished_ring_`
- Calls `close_segment()` asynchronously
- Prevents blocking the replay thread

#### **Spin-wait + yield scheduling**
To guarantee low jitter:
- Uses `cpu_relax()` inside busy loops  
- Yields occasionally to avoid monopolizing CPU time  

This results in extremely predictable timing and minimal OS jitter.

---

### Lock‑Free Rings

Two SPSC rings connect Manager ↔ Worker:

| Ring | Producer | Consumer | Purpose |
|------|----------|----------|---------|
| `prepared_ring_` | Worker | Manager | Deliver validated segments |
| `finished_ring_` | Manager | Worker | Close exhausted segments |

Properties:
- **Single-producer/single-consumer** → no locks  
- **Cacheline‑separated state** → no false sharing  
- **Deterministic timing** suitable for real‑time workloads  

---

### SegmentReader (Strict Mode)

Each WAL segment is read by a strict reader that performs:

- Full header parsing  
- Validation of:
  - Block header consistency  
  - Event count  
  - Local checksum  
  - Chained checksum  
- Sparse index construction for fast seeking  
- Sequential event iteration  

Strict mode guarantees:
- No skipping events  
- No tolerance for corruption  
- Deterministic, reproducible replay

If any integrity issue appears, the reader marks the segment invalid.

---

### Diagnostic Mode (`WalDiagnosticReader` / Manager)

Designed for non‑strict analysis:
- May continue after corruption
- Useful for debugging or tooling
- Does *not* affect strict-mode replay semantics

This class is intentionally separate to keep strict recovery fast.

---

## Execution Flow

### Step 1 — Initialization
`Manager.initialize()` scans the WAL directory:

1. Read header of each `.wal` file  
2. Sort segments by `first_event_id`  
3. Prepare ordered list of segments  

---

### Step 2 — Start Recovery

`Manager.resume_from_event(event_id)`:

1. Find the segment containing this event  
2. Open it with `SegmentReader`  
3. Seek to requested event  
4. Launch `SegmentPreloader` with **the remaining segments**  

After this point the Manager only reads events; background I/O is fully delegated.

---

### Step 3 — Replay Loop (`next()`)

At each call:

1. Try `reader_->next(ev)`
2. If it succeeds → fast path
3. If it fails (EOF):
   - Push exhausted reader into `finished_ring_`
   - Pop a preloaded next reader
   - Continue

Fallback path:
- If no preloaded reader exists, synchronously open next segment

This fallback is extremely rare because the worker stays ahead by design.

---

## Performance Characteristics

| Operation | Typical Cost |
|----------|---------------|
| Event decode | 10–40 ns |
| Block switch | < 80 ns |
| Segment switch (preloaded) | ~2–5 µs |
| Segment open + verify (sync) | 200–400 ms |
| Preloader throughput | 3–6 segments/sec |

Key advantages:
- Segment transitions never interrupt replay  
- No syscalls on hot path  
- No locks anywhere in replay thread  
- Worker thread amortizes all expensive I/O  

---

## Telemetry Integration

Recovery telemetry includes:

### SegmentReader metrics
- Block validation duration  
- Seek operations  
- Segment open/close latencies  

### SegmentPreloader metrics
- Preload duration per segment  
- Finished-segment cleanup timings  

### Manager metrics
- Next-event latency histogram  
- Seek + segment transition durations  
- Segment header read timings  

These allow real-time observability of WAL replay performance.

---

## Reliability Guarantees

Strict recovery ensures:

✔ Full WAL integrity verification  
✔ Deterministic event ordering  
✔ No skipped or duplicated events  
✔ Graceful handling of corrupted or truncated segments  
✔ Safe asynchronous cleanup  

All background work is partitioned such that corruption in future segments cannot affect active recovery.

---

## Summary Diagram (Conceptual)

```
 ┌───────────────┐        preloaded segments       ┌──────────────────────────┐
 │ Segment Files │  ────────────────────────────>  │ SegmentPreloader (worker)│
 └───────────────┘                                 └───────────┬──────────────┘
                                                               │
                                                       prepared_ring_
                                                               │
                                                       ┌───────▼────────┐
                                                       │     Manager    │
                                                       │ (active replay)│
                                                       └───────┬────────┘
                                                               │
                                                        finished_ring_
                                                               │
                                              ┌────────────────▼───────────────┐
                                              │   Worker closes exhausted segs │
                                              └────────────────────────────────┘
```

---

## Conclusion

This architecture ensures:

- **Maximum throughput**
- **Deterministic behavior**
- **Full integrity validation**
- **Zero-latency segment transitions**
- **No blocking I/O on the replay path**

It is optimized for high-frequency trading, matching engines, and other latency‑critical systems.

---

## Related components

[`recovery::SegmentReader`](./segment_reader.md)
[`recovery::Telemetry`](./telemetry.md)

[`recovery::worker::SegmentPreloader`](./worker/segment_preloader.md)

---

👉 Back to [`WAL Recovery System — Overview`](../recovery_overview.md)
