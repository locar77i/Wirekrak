#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <utility>
#include <cstdint>
#include <iostream>

#include "flashstrike/constants.hpp"
#include "lcr/system/monotonic_clock.hpp"

using lcr::system::monotonic_clock;

namespace lcr {
namespace metrics {
namespace snapshot {

template <typename MetricsType>
class Manager {
public:
    struct SnapshotInfo {
        const MetricsType* data;   // pointer to stable snapshot
        uint64_t version;          // monotonic version
        uint64_t timestamp_ns;     // when snapshot was taken
        uint64_t age_ms(uint64_t now_ns) const noexcept {
            return (now_ns - timestamp_ns) / 1'000'000ULL;
        }
    };

    explicit Manager(
        MetricsType& live_metrics,
        std::function<void(const SnapshotInfo&)> on_snapshot = nullptr
    )
        : live_metrics_(live_metrics)
        , on_snapshot_(std::move(on_snapshot))
    {}

    // -------------------------------------------------------------------------
    //  Take snapshot manually (call every 5 seconds)
    // -------------------------------------------------------------------------
    inline void take_snapshot() noexcept {
        uint64_t local_version = version_.load(std::memory_order_relaxed) + 1;

        const int next_idx = 1 - active_index_.load(std::memory_order_relaxed);
        auto& buf = buffers_[next_idx];

        // Copy metrics (typically ~3–5 µs)
        copy_metrics_(buf.metrics, live_metrics_);

        buf.version.store(local_version, std::memory_order_relaxed);
        buf.timestamp_ns.store(clock_.now_ns(), std::memory_order_relaxed);

        // Publish snapshot
        version_.store(local_version, std::memory_order_relaxed);
        active_index_.store(next_idx, std::memory_order_release);

        // Optional callback
        if (on_snapshot_) {
            on_snapshot_(snapshot());
        }
    }

    // -------------------------------------------------------------------------
    // Get stable snapshot
    // -------------------------------------------------------------------------
    inline SnapshotInfo snapshot() const noexcept {
        const int idx = active_index_.load(std::memory_order_acquire);
        return {
            .data = &buffers_[idx].metrics,
            .version = buffers_[idx].version.load(std::memory_order_relaxed),
            .timestamp_ns = buffers_[idx].timestamp_ns.load(std::memory_order_relaxed)
        };
    }

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
    std::atomic<uint64_t> version_{0};

    std::function<void(const SnapshotInfo&)> on_snapshot_;

    inline void copy_metrics_(MetricsType& dst, const MetricsType& src) noexcept {
        src.copy_to(dst);
    }
};

} // namespace snapshot
} // namespace metrics
} // namespace lcr
