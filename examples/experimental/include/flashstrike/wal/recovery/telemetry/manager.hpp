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

struct alignas(64) Manager {
    alignas(64) stats::operation64 read_segment_header{};
    alignas(64) stats::operation64 resume_from_event{};
    alignas(64) stats::duration64 seek_event{};
    alignas(64) stats::duration64 next_event{};
    alignas(64) latency_histogram next_event_histogram{};

    // Constructor
    Manager() = default;
    // Disable copy/move semantics
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) noexcept = delete;
    Manager& operator=(Manager&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(Manager& other) const noexcept {
        read_segment_header.copy_to(other.read_segment_header);
        resume_from_event.copy_to(other.resume_from_event);
        seek_event.copy_to(other.seek_event);
        next_event.copy_to(other.next_event);
        next_event_histogram.copy_to(other.next_event_histogram);
    }

    // Dump metrics to ostream (human-readable)
    inline void dump(const std::string& label, std::ostream& os) const noexcept {
        os  << "[" << label << " Metrics] Snapshot:\n";
        os << "-----------------------------------------------------------------\n";
        os << " Read segment header: " << read_segment_header.str(time_unit::milliseconds, time_unit::milliseconds) << "\n";
        os << " Resume from event  : " << resume_from_event.str(time_unit::seconds, time_unit::milliseconds) << "\n";
        os << " Seek event         : " << seek_event.str(time_unit::microseconds, time_unit::microseconds) << "\n";
        os << " Next event         : " << next_event.str(time_unit::seconds, time_unit::microseconds) << "\n";
        os << " -> " << next_event_histogram.compute_percentiles().str() << "\n";
        os << "-----------------------------------------------------------------\n";
    }

    // Metrics collector
    template <typename Collector>
    inline void collect(const std::string& prefix, Collector& collector) const noexcept {
        read_segment_header.collect(prefix + "_read_segment_header", collector);
        resume_from_event.collect(prefix + "_resume_from_event", collector);
        seek_event.collect(prefix + "_seek_event", collector);
        next_event.collect(prefix + "_next_event", collector);
        next_event_histogram.collect(prefix + "_next_event_histogram", collector);
    }
};
// -----------------------------
// Compile-time verification
// -----------------------------
static_assert(sizeof(Manager) % 64 == 0, "Manager size must be multiple of 64 bytes");
static_assert(alignof(Manager) == 64, "Manager must be aligned to 64 bytes");
static_assert(offsetof(Manager, read_segment_header) % 64 == 0, "read_segment_header must start at a cache-line boundary");
static_assert(offsetof(Manager, resume_from_event) % 64 == 0, "resume_from_event must start at a cache-line boundary");
static_assert(offsetof(Manager, seek_event) % 64 == 0, "seek_event must start at a cache-line boundary");
static_assert(offsetof(Manager, next_event) % 64 == 0, "next_event must start at a cache-line boundary");
static_assert(offsetof(Manager, next_event_histogram) % 64 == 0, "next_event_histogram must start at a cache-line boundary");
// -----------------------------


class ManagerUpdater {
public:
    explicit ManagerUpdater(Manager& metrics) : metrics_(metrics) {}
    // ------------------------------------------------------------------------

    inline void on_read_segment_header(uint64_t start_ns, Status status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.read_segment_header.record(start_ns, end_ns, status == Status::OK);
    }

    inline void on_resume_from_event(uint64_t start_ns, Status status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.resume_from_event.record(start_ns, end_ns, status == Status::OK);
    }

    inline void on_seek_event(uint64_t start_ns) const noexcept {
        metrics_.seek_event.record(start_ns, monotonic_clock::instance().now_ns());
    }

    inline void on_next_event(uint64_t start_ns) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.next_event.record(start_ns, end_ns);
        metrics_.next_event_histogram.record(start_ns, end_ns);
    }
    
private:
    Manager& metrics_;
};


} // namespace telemetry
} // namespace recovery
} // namespace wal
} // namespace flashstrike
