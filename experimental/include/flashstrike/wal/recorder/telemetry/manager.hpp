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

struct alignas(64) Manager {
    alignas(64) stats::operation64 init_active_segment{};
    alignas(64) stats::operation64 append_event{};
    alignas(64) latency_histogram append_event_histogram{};
    alignas(64) stats::duration64 segment_rotation{};
    alignas(64) stats::duration64 work_planning{};
    alignas(64) stats::duration64 persist_current_segment{};
    alignas(64) gauge64 persistence_hot_segments{};
    gauge64 persistence_cold_segments{};
    char pad_[64 - (2 * sizeof(gauge64)) % 64] = {0};

    // Constructor
    Manager() = default;
    // Disable copy/move semantics
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) noexcept = delete;
    Manager& operator=(Manager&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(Manager& other) const noexcept {
        init_active_segment.copy_to(other.init_active_segment);
        append_event.copy_to(other.append_event);
        append_event_histogram.copy_to(other.append_event_histogram);
        segment_rotation.copy_to(other.segment_rotation);
        work_planning.copy_to(other.work_planning);
        persist_current_segment.copy_to(other.persist_current_segment);
        persistence_hot_segments.copy_to(other.persistence_hot_segments);
        persistence_cold_segments.copy_to(other.persistence_cold_segments);
    }

    // Dump metrics to ostream (human-readable)
    inline void dump(const std::string& label, std::ostream& os) const noexcept {
        os  << "[" << label << " Metrics] Snapshot:\n";
        os << "-----------------------------------------------------------------\n";
        os << " Init active segment   : " << init_active_segment.str() << "\n";
        os << " Append event          : " << append_event.str(time_unit::milliseconds, time_unit::microseconds) << "\n";
        os << " -> " << append_event_histogram.compute_percentiles().str() << "\n";
        os << " Rotation              : " << segment_rotation.str(time_unit::microseconds, time_unit::microseconds) << "\n";
        os << " Work planning         : " << work_planning.str(time_unit::microseconds, time_unit::microseconds) << "\n";
        os << " Persist current segm. : " << persist_current_segment.str(time_unit::milliseconds, time_unit::milliseconds) << "\n";
        os << " Current hot segments  : " << persistence_hot_segments.load() << "\n";
        os << " Current cold segments : " << persistence_cold_segments.load() << "\n";
        os << "-----------------------------------------------------------------\n";
    }

    // Metrics collector
    template <typename Collector>
    inline void collect(const std::string& prefix, Collector& collector) const noexcept {
        init_active_segment.collect(prefix + "_restore_or_create_segment", collector);
        append_event.collect(prefix + "_append_event", collector);
        append_event_histogram.collect(prefix + "_append_event_histogram", collector);
        segment_rotation.collect(prefix + "_segment_rotation", collector);
        work_planning.collect(prefix + "_work_planning", collector);
        persist_current_segment.collect(prefix + "_persist_current_segment", collector);
        persistence_hot_segments.collect(prefix + "_persistence_hot_segments", "Last number of hot segments", collector);
        persistence_cold_segments.collect(prefix + "_persistence_cold_segments", "Last number of cold segments", collector);
    }
};
// Compile-time verification
static_assert(sizeof(Manager) % 64 == 0, "Manager size must be multiple of 64 bytes");
static_assert(alignof(Manager) == 64, "Manager must be aligned to 64 bytes");
static_assert(offsetof(Manager, init_active_segment) % 64 == 0, "init_active_segment must start at a cache-line boundary");
static_assert(offsetof(Manager, append_event) % 64 == 0, "append_event must start at a cache-line boundary");
static_assert(offsetof(Manager, append_event_histogram) % 64 == 0, "append_event_histogram must start at a cache-line boundary");
static_assert(offsetof(Manager, segment_rotation) % 64 == 0, "segment_rotation must start at a cache-line boundary");
static_assert(offsetof(Manager, work_planning) % 64 == 0, "work_planning must start at a cache-line boundary");
static_assert(offsetof(Manager, persist_current_segment) % 64 == 0, "persist_current_segment must start at a cache-line boundary");
static_assert(offsetof(Manager, persistence_hot_segments) % 64 == 0, "persistence_hot_segments must start at a cache-line boundary");

class ManagerUpdater {
public:
    explicit ManagerUpdater(Manager& metrics) : metrics_(metrics) {}

    inline void on_init_active_segment(uint64_t start_ns, Status status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.init_active_segment.record(start_ns, end_ns, status == Status::OK);
    }

    inline void on_append_event(uint64_t start_ns, Status status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.append_event.record(start_ns, end_ns, status == Status::OK);
        metrics_.append_event_histogram.record(start_ns, end_ns);
    }

    inline void on_segment_rotation(uint64_t start_ns) const noexcept {
        metrics_.segment_rotation.record(start_ns, monotonic_clock::instance().now_ns());
    }

    inline void on_work_planning(uint64_t start_ns) const noexcept {
        metrics_.work_planning.record(start_ns, monotonic_clock::instance().now_ns());
    }

    inline void on_persist_current_segment(uint64_t start_ns) const noexcept {
        metrics_.persist_current_segment.record(start_ns, monotonic_clock::instance().now_ns());
    }

private:
    Manager& metrics_;
};


} // namespace telemetry
} // namespace recorder
} // namespace wal
} // namespace flashstrike
