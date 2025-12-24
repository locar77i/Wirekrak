# Snapshotter

## Summary
`lcr::metrics::runtime::snapshotter<T>` is a lightweight, double-buffered snapshot utility that produces **stable, coherent, read-only replicas** of a live metrics structure at a configurable cadence.  
It is specifically designed for HFT systems where the writer(s) update metrics in hot paths and multiple readers (dashboards, exporters, CLI tools) must access metrics without causing cache-line contention or inconsistent multi-field reads.

---

## 1. Purpose
Reading hot-path metrics directly creates cache-line contention and can degrade matching engine latency. The snapshotter provides a cheap, isolated copy of the live metrics so readers never touch the hot data.

Use cases:
- Dashboards and UIs polling frequently
- Metrics exporters pushing to observability backends
- CLI inspection tools that require coherent multi-field snapshots
- Any scenario with multiple high-frequency readers

Avoid using the snapshotter if:
- There is only one reader and low polling frequency.
- Metrics are trivial and low-volume (single counters rarely accessed).

---

## 2. High-Level Design
- **Double-buffered**: two aligned buffers (`buffers_[0]` and `buffers_[1]`) hold metric copies.
- **Atomic publish**: `active_index_` atomically points to the currently published buffer.
- **Background thread**: copies live metrics into the inactive buffer at a configured interval, updates version and timestamp, then atomically flips `active_index_`.
- **Readers**: call `snapshot()` to fetch a small `SnapshotInfo` referencing the published buffer; they do not modify any shared hot-path memory.
- **Adaptive busy-wait**: the snapshot thread uses a busy-wait loop with adaptive spinning to achieve precise cadence and low jitter.

```
[Live metrics] --copy--> buffers_[inactive] --atomic flip--> readers read buffers_[active]
```

---

## 3. API (public interface)
```cpp
template <typename MetricsType>
class snapshotter {
public:
    struct SnapshotInfo {
        const MetricsType* data;    // pointer to snapshot data
        uint64_t version;           // snapshot version (monotonic)
        uint64_t timestamp_ns;      // when snapshot was taken
        uint64_t age_ms(uint64_t now_ns) const noexcept;
    };

    // Constructor:
    // - live_metrics: reference to the live metrics instance
    // - interval: periodic capture interval (default 1000ms)
    // - on_snapshot: optional callback invoked after publish
    snapshotter(MetricsType& live_metrics,
                std::chrono::milliseconds interval = std::chrono::milliseconds(1000),
                std::function<void(const SnapshotInfo&)> on_snapshot = nullptr);

    void start(); // spawns background snapshot thread
    void stop() noexcept; // stops and joins background thread
    SnapshotInfo snapshot() const noexcept; // get current stable snapshot info
    ~snapshotter(); // stops thread if running
};
```

Key requirement for `MetricsType`:
- Must expose a `void copy_to(MetricsType& other) const noexcept;` member or equivalent method used by `snapshotter` to copy the live metrics into the snapshot buffer.

---

## 4. Semantics & Memory Ordering
- `SnapshotBuffer` is `alignas(64)` to avoid false sharing.
- Publication uses `active_index_.store(..., std::memory_order_release)`.
- Readers load `active_index_.load(std::memory_order_acquire)` to guarantee visibility.
- Version and timestamp use relaxed atomic stores for efficiency; snapshot ordering is guaranteed by the release/acquire pairing on the index flip.

---

## 5. Timing Model & Adaptive Waiting
- Snapshots target absolute time boundaries (`next_target_ns`) instead of sleeping relative intervals to reduce drift.
- The busy-wait loop:
  - Spins with `cpu_relax()` for `max_spins` iterations.
  - Calls `std::this_thread::yield()` when spins exceed the threshold.
  - Adapts `max_spins` based on observed wake latency:
    - if within <1ms, increase spins (more busy waiting)
    - if >10ms, decrease spins (less busy waiting)
- This design balances CPU usage vs. precision and is suitable for HFT environments where short, consistent latencies are preferred.

---

## 6. Example Usage
```cpp
// Suppose Telemetry is the top-level metrics struct
Telemetry live_metrics;
lcr::metrics::runtime::snapshotter<Telemetry> snap(live_metrics, std::chrono::milliseconds(500),
    [](const auto& info){
        // optional: push to exporter or notify
        // e.g., logger->info("snapshot version {} ts {}", info.version, info.timestamp_ns);
    });

snap.start();

// In reader thread or HTTP handler:
auto s = snap.snapshot();
const Telemetry& view = *s.data;
// Read stable fields:
auto samples = view.manager_metrics.process.samples();
// compute age:
uint64_t age = s.age_ms(monotonic_clock::instance().now_ns());

snap.stop();
```

---

## 7. Performance Notes
- **Writer overhead:** zero — writers operate on their live metrics; snapshotter reads the live metrics.
- **Snapshot thread overhead:** `memcpy`-like copy of the metrics structure plus atomic flip; cost proportional to metrics size.
- **Reader cost:** trivial — pointer dereference to snapshot buffer and reads.

Recommendations:
- Keep `MetricsType` reasonably sized (avoid huge dynamic allocations inside metrics).
- If metrics struct is large, consider field-wise selective copying or divide into multiple snapshotters for different subsystems.
- Choose snapshot interval considering staleness vs. CPU: 100-1000 ms is common for dashboards; 10-100 ms for high-frequency monitoring.

---

## 8. Safety, Edge Cases & Troubleshooting
- **Shutdown:** ensure `stop()` is called before program exit to join the thread.
- **Copy correctness:** `copy_to()` should be exception-free and noexcept. Any heavy computations inside `copy_to` may delay the snapshot thread.
- **Time skew:** If the snapshot thread gets scheduled late repeatedly, the adaptive logic will skip missed intervals; monitor `age_ms()` to detect lag.
- **Multiple snapshotters:** running many snapshotters in parallel increases CPU cost — the copies happen concurrently, so coordinate intervals if necessary.

---

## 9. Deployment Tips
- For dashboarding (Prometheus push/pull), have the snapshotter callback feed an aggregator rather than having the exporter read live metrics directly.
- For testing, set the interval to a short period (e.g., 100ms) and measure age and CPU. For production dashboards, 500-1000ms is a reasonable trade-off.
- Use the snapshot version to detect dropped snapshots on the consumer side.

---

## 10. Integration with FlashStrike Telemetry
- Instantiate a snapshotter for the top-level `matching_engine::Telemetry` (contains `Init`, `Manager`, `PriceLevelStore`, `LowLevel`) to get a single coherent view.
- Alternatively, create separate snapshotters for `Manager` and `LowLevel` if they are consumed independently and have different freshness requirements.

---

## 11. Troubleshooting Checklist
- If `snapshot().data` is null: ensure snapshotter started and published at least one snapshot.
- If `age_ms()` grows large: the snapshot thread may be starved — check CPU and `max_spins` tuning.
- If readers see inconsistent data (unlikely): verify `MetricsType::copy_to` correctly copies all fields.
- If snapshots are too heavy: split metrics into multiple smaller snapshotters.

---

## 12. Appendix

### A. Recommended defaults
- Default interval: **1000 ms**
- Initial `max_spins`: `SPINS_GUESS`
- Alignment: 64 bytes for buffers
- on_snapshot callback: optional, used to trigger exporters
