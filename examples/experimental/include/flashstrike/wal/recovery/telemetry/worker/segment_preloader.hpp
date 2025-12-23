#pragma once

#include "flashstrike/wal/types.hpp"
#include "lcr/metrics.hpp"
#include "lcr/system/monotonic_clock.hpp"

using lcr::time_unit;
using namespace lcr::metrics;


namespace flashstrike {
namespace wal {
namespace recovery {
namespace telemetry {
namespace worker {

struct alignas(64) SegmentPreloader {
    alignas(64) stats::operation64 preload_segment{};
    alignas(64) stats::operation64 finish_segment{};

    // Constructor
    SegmentPreloader() = default;
    // Disable copy/move semantics
    SegmentPreloader(const SegmentPreloader&) = delete;
    SegmentPreloader& operator=(const SegmentPreloader&) = delete;
    SegmentPreloader(SegmentPreloader&&) noexcept = delete;
    SegmentPreloader& operator=(SegmentPreloader&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(SegmentPreloader& other) const noexcept {
        preload_segment.copy_to(other.preload_segment);
        finish_segment.copy_to(other.finish_segment);
    }

    // Dump metrics to ostream (human-readable)
    inline void dump(const std::string& label, std::ostream& os) const noexcept {
        os  << "[" << label << " Metrics] Snapshot:\n";
        os << "-----------------------------------------------------------------\n";
        os << " Preload segment: " << preload_segment.str(time_unit::seconds, time_unit::milliseconds) << "\n";
        os << " Finish segment : " << finish_segment.str(time_unit::milliseconds, time_unit::milliseconds) << "\n";
        os << "-----------------------------------------------------------------\n";
    }

    // Metrics collector
    template <typename Collector>
    inline void collect(const std::string& prefix, Collector& collector) const noexcept {
        // Push the current label before serializing
        collector.push_label("subsystem", "wal_recovery_worker");
        // Serialize recovery worker metrics
        preload_segment.collect(prefix + "_preload_segment", collector);
        finish_segment.collect(prefix + "_finish_segment", collector);
        // Pop the label after serialization
        collector.pop_label();
    }
};
// Compile-time verification
static_assert(sizeof(SegmentPreloader) % 64 == 0, "SegmentPreloader size must be multiple of 64 bytes");
static_assert(alignof(SegmentPreloader) == 64, "SegmentPreloader must be aligned to 64 bytes");
static_assert(offsetof(SegmentPreloader, preload_segment) % 64 == 0, "preload_segment must start at a cache-line boundary");
static_assert(offsetof(SegmentPreloader, finish_segment) % 64 == 0, "finish_segment must start at a cache-line boundary");
// -----------------------------


class SegmentPreloaderUpdater {
public:
    explicit SegmentPreloaderUpdater(SegmentPreloader& metrics) : metrics_(metrics) {}
    // ------------------------------------------------------------------------

    inline void on_preload_segment(uint64_t start_ns, Status status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.preload_segment.record(start_ns, end_ns, status == Status::OK);
    }

    inline void on_finish_segment(uint64_t start_ns, Status status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.finish_segment.record(start_ns, end_ns, status == Status::OK);
    }
    
private:
    SegmentPreloader& metrics_;
};


} // namespace worker
} // namespace telemetry
} // namespace recovery
} // namespace wal
} // namespace flashstrike
