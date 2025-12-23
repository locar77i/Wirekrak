#pragma once

#include <string>
#include <ostream>

#include "flashstrike/wal/types.hpp"
#include "lcr/metrics.hpp"
#include "lcr/system/monotonic_clock.hpp"

using lcr::time_unit;
using namespace lcr::metrics;


namespace flashstrike {
namespace wal {
namespace recorder {
namespace telemetry {
namespace worker {

struct alignas(64) SegmentPreparer {
    alignas(64) stats::duration64 get_next_segment{};

    // Constructor
    SegmentPreparer() = default;
    // Disable copy/move semantics
    SegmentPreparer(const SegmentPreparer&) = delete;
    SegmentPreparer& operator=(const SegmentPreparer&) = delete;
    SegmentPreparer(SegmentPreparer&&) noexcept = delete;
    SegmentPreparer& operator=(SegmentPreparer&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(SegmentPreparer& other) const noexcept {
        get_next_segment.copy_to(other.get_next_segment);
    }

    // Dump metrics to ostream (human-readable)
    inline void dump(const std::string& label, std::ostream& os) const noexcept {
        os  << "[" << label << " Metrics] Snapshot:\n";
        os << "-----------------------------------------------------------------\n";
        os << " Get next segment: " << get_next_segment.str(time_unit::microseconds, time_unit::microseconds) << "\n";
        os << "-----------------------------------------------------------------\n";
    }

    // Metrics collector
    template <typename Collector>
    inline void collect(const std::string& prefix, Collector& collector) const noexcept {
        // Push the current label before serializing
        collector.push_label("subsystem", "wal_prepare_worker");
        // Serialize prepare worker metrics
        get_next_segment.collect(prefix + "_get_next_segment_ns", collector);
        // Pop the label after serialization
        collector.pop_label();
    }
};
// Compile-time verification
static_assert(sizeof(SegmentPreparer) % 64 == 0, "SegmentPreparer size must be multiple of 64 bytes");
static_assert(alignof(SegmentPreparer) == 64, "SegmentPreparer must be aligned to 64 bytes");
static_assert(offsetof(SegmentPreparer, get_next_segment) % 64 == 0, "get_next_segment must start at a cache-line boundary");

class SegmentPreparerUpdater {
public:
    explicit SegmentPreparerUpdater(SegmentPreparer& metrics) : metrics_(metrics) {}

    inline void on_get_next_segment(uint64_t start_ns) const noexcept {
        metrics_.get_next_segment.record(start_ns, monotonic_clock::instance().now_ns());
    }

private:
    SegmentPreparer& metrics_;
};


} // namespace worker
} // namespace telemetry
} // namespace recorder
} // namespace wal
} // namespace flashstrike
