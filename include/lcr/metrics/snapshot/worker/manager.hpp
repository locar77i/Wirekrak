#pragma once

#include <atomic>
#include <thread>
#include <functional>
#include <algorithm>
#include <chrono>
#include <utility>
#include <cstdint>
#include <iostream>

#include "flashstrike/constants.hpp"
#include "lcr/system/cpu_relax.hpp"
#include "lcr/system/monotonic_clock.hpp"

using lcr::system::monotonic_clock;


namespace lcr {
namespace metrics {
namespace snapshot {
namespace worker {


// If there’s only one reader, snapshotting adds no real benefit. it just moves the interference from the reader to another thread doing the same work.
// The snapshot thread only pays off when:
// - We have multiple readers (UI dashboards, metrics exporters, CLI tools, etc.),
// - Or the readers poll at very high frequency (e.g. hundreds of times per second),
// - When we want multi-field atomicity — a coherent snapshot of all counters together.
// In those cases, the snapshot thread acts like a read replicator:
// - it does one batch copy (cache-local, fast)
// - and all readers access that stable, read-only view.
// That way, only one thread (the Manager) touches the hot metrics area — isolating the actual writer thread’s cache lines.
//
// Future work: extend it to support multiple readers (each with their own “last seen version”) efficiently — a multi-subscriber monitoring setup
template <typename MetricsType>
class Manager {
public:
    struct SnapshotInfo {
        const MetricsType* data;         // pointer to current snapshot
        uint64_t version;                // monotonically increasing version
        uint64_t timestamp_ns;           // monotonic timestamp when taken
        uint64_t age_ms(uint64_t now_ns) const noexcept {
            return (now_ns - timestamp_ns) / 1'000'000ULL;
        }
    };

    explicit Manager(MetricsType& live_metrics, std::chrono::milliseconds interval = std::chrono::milliseconds(1000), std::function<void(const SnapshotInfo&)> on_snapshot = nullptr) 
        : live_metrics_(live_metrics)
        , snapshot_interval_ms_(interval)
        , on_snapshot_(std::move(on_snapshot)) {}

    // Start background thread
    void start() {
        stop_flag_.store(false, std::memory_order_release);
        snapshot_thread_ = std::thread([this] { snapshot_loop_(); });
    }

    // Stop background thread
    void stop() noexcept {
        stop_flag_.store(true, std::memory_order_release);
        if (snapshot_thread_.joinable())
            snapshot_thread_.join();
    }

    // Get stable snapshot info
    SnapshotInfo snapshot() const noexcept {
        const int idx = active_index_.load(std::memory_order_acquire);
        return {
            .data = &buffers_[idx].metrics,
            .version = buffers_[idx].version.load(std::memory_order_relaxed),
            .timestamp_ns = buffers_[idx].timestamp_ns.load(std::memory_order_relaxed),
        };
    }

    ~Manager() { stop(); }

private:
    struct alignas(64) SnapshotBuffer {
        MetricsType metrics;
        std::atomic<uint64_t> version{0};
        std::atomic<uint64_t> timestamp_ns{0};
    };

    monotonic_clock& clock_ = monotonic_clock::instance();
    MetricsType& live_metrics_;
    SnapshotBuffer buffers_[2];
    alignas(64) std::atomic<int> active_index_{0};
    std::atomic<bool> stop_flag_{false};
    std::chrono::milliseconds snapshot_interval_ms_;
    std::thread snapshot_thread_;
    std::function<void(const SnapshotInfo&)> on_snapshot_;
    // ---------------------------------------------------------------------------

    void snapshot_loop_() noexcept {
        uint64_t local_version = 0;
        while (!stop_flag_.load(std::memory_order_acquire)) {
            const int next_idx = 1 - active_index_.load(std::memory_order_relaxed);
            auto& buf = buffers_[next_idx];
            // Take snapshot
            copy_metrics_(buf.metrics, live_metrics_);
            buf.version.store(++local_version, std::memory_order_relaxed);
            buf.timestamp_ns.store(clock_.now_ns(), std::memory_order_relaxed);
            // Publish snapshot
            active_index_.store(next_idx, std::memory_order_release);
            // Callback if any
            if (on_snapshot_) {
                on_snapshot_(snapshot());
            }
            std::this_thread::sleep_for(snapshot_interval_ms_);
        }
    }

    static void copy_metrics_(MetricsType& dst, const MetricsType& src) noexcept {
        auto start_ns = monotonic_clock::instance().now_ns();
        src.copy_to(dst);
        auto end_ns = monotonic_clock::instance().now_ns();
        std::cout << "[Snapshotter] Copied metrics snapshot in " << (end_ns - start_ns) << " ns\n";
    }
};

} // namespace worker
} // namespace snapshot
} // namespace metrics
} // namespace lcr
