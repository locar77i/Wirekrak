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

struct alignas(64) SegmentMaintainer {
    alignas(64) stats::life_cycle persistence_lifecycle{};
    alignas(64) stats::operation64 maintenance_retention{};
    alignas(64) stats::operation64 maintenance_compression{};
    alignas(64) stats::operation64 maintenance_deletion{};
    alignas(64) constant_gauge_u64 persistence_max_hot_segments{0};   // Constant
    constant_gauge_u64 persistence_max_cold_segments{0};  // Constant
    char pad_[64 - (2 * sizeof(constant_gauge_u64)) % 64] = {0};

    // Constructor
    SegmentMaintainer() = default;
    // Disable copy/move semantics
    SegmentMaintainer(const SegmentMaintainer&) = delete;
    SegmentMaintainer& operator=(const SegmentMaintainer&) = delete;
    SegmentMaintainer(SegmentMaintainer&&) noexcept = delete;
    SegmentMaintainer& operator=(SegmentMaintainer&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(SegmentMaintainer& other) const noexcept {
        persistence_lifecycle.copy_to(other.persistence_lifecycle);
        maintenance_retention.copy_to(other.maintenance_retention);
        maintenance_compression.copy_to(other.maintenance_compression);
        maintenance_deletion.copy_to(other.maintenance_deletion);
        persistence_max_hot_segments.copy_to(other.persistence_max_hot_segments);
        persistence_max_cold_segments.copy_to(other.persistence_max_cold_segments);
    }

    // Dump metrics to ostream (human-readable)
    inline void dump(const std::string& label, std::ostream& os) const noexcept {
        os  << "[" << label << " Metrics] Snapshot:\n";
        os << "-----------------------------------------------------------------\n";
        os << " Timing / load balancing: " << persistence_lifecycle.str(time_unit::seconds, time_unit::milliseconds) << "\n";
        os << " Current hot segments   : " << persistence_max_hot_segments.load() << "\n";
        os << " Current cold segments  : " << persistence_max_cold_segments.load() << "\n";
        os << " Retention metrics      : " << maintenance_retention.str() << "\n";
        os << " Compression metrics    : " << maintenance_compression.str() << "\n";
        os << " Deletion metrics       : " << maintenance_deletion.str() << "\n";
        os << "-----------------------------------------------------------------\n";
    }

    // Metrics collector
    template <typename Collector>
    inline void collect(const std::string& prefix, Collector& collector) const noexcept {
        // Push the current label before serializing
        collector.push_label("subsystem", "wal_persistence_worker");
        // Serialize persistence worker metrics
        persistence_lifecycle.collect(prefix + "_persistence_lifecycle", collector);
        maintenance_retention.collect(prefix + "_maintenance_retention", collector);
        maintenance_compression.collect(prefix + "_maintenance_compression", collector);
        maintenance_deletion.collect(prefix + "_maintenance_deletion", collector);
        persistence_max_hot_segments.collect(prefix + "_persistence_max_hot_segments", "Maximum number of hot segments", collector);
        persistence_max_cold_segments.collect(prefix + "_persistence_max_cold_segments", "Maximum number of cold segments", collector);
        // Pop the label after serialization
        collector.pop_label();
    }
};
// Compile-time verification
static_assert(sizeof(SegmentMaintainer) % 64 == 0, "SegmentMaintainer size must be multiple of 64 bytes");
static_assert(alignof(SegmentMaintainer) == 64, "SegmentMaintainer must be aligned to 64 bytes");
static_assert(offsetof(SegmentMaintainer, persistence_lifecycle) % 64 == 0, "persistence_lifecycle must start at a cache-line boundary");
static_assert(offsetof(SegmentMaintainer, maintenance_retention) % 64 == 0, "maintenance_retention must start at a cache-line boundary");
static_assert(offsetof(SegmentMaintainer, maintenance_compression) % 64 == 0, "maintenance_compression must start at a cache-line boundary");
static_assert(offsetof(SegmentMaintainer, maintenance_deletion) % 64 == 0, "maintenance_deletion must start at a cache-line boundary");
static_assert(offsetof(SegmentMaintainer, persistence_max_hot_segments) % 64 == 0, "persistence_max_hot_segments must start at a cache-line boundary");

class SegmentMaintainerUpdater {
public:
    explicit SegmentMaintainerUpdater(SegmentMaintainer& metrics) : metrics_(metrics) {}

    // ------------------------------------------------------------------------
    // Main writer thread
    // ------------------------------------------------------------------------
    inline void on_persistence_loop(bool did_work, uint64_t start_ns, uint64_t sleep_time) const noexcept {
        metrics_.persistence_lifecycle.record(start_ns, monotonic_clock::instance().now_ns(), sleep_time*1'000'000ULL, did_work);
    }

    inline void on_hot_segment_retention(bool ok, uint64_t start_ns) const noexcept {
        metrics_.maintenance_retention.record(start_ns, monotonic_clock::instance().now_ns(), ok);
    }

    inline void on_hot_segment_compression(bool ok, uint64_t start_ns) const noexcept {
        metrics_.maintenance_compression.record(start_ns, monotonic_clock::instance().now_ns(), ok);
    }

    inline void on_cold_segment_deletion(bool ok, uint64_t start_ns) const noexcept {
        metrics_.maintenance_deletion.record(start_ns, monotonic_clock::instance().now_ns(), ok);
    }

    inline void set_max_segments(size_t max_segments) const noexcept {
        metrics_.persistence_max_hot_segments.set(max_segments);
    }

    inline void set_max_compressed_segments(size_t max_compressed_segments) const noexcept {
        metrics_.persistence_max_cold_segments.set(max_compressed_segments);
    }

private:
    SegmentMaintainer& metrics_;
};


} // namespace worker
} // namespace telemetry
} // namespace recorder
} // namespace wal
} // namespace flashstrike
