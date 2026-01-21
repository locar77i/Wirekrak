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

struct alignas(64) SegmentReader {
    alignas(64) stats::operation64 open_segment{};
    alignas(64) stats::operation64 close_segment{};
    alignas(64) stats::operation64 verify_segment{};
    alignas(64) counter64 total_header_checksum_failures{};
    counter64 total_block_checksum_failures{};
    counter64 total_chained_checksum_failures{};
    counter64 total_validation_failures{};
    char pad1_[64 - (4 * sizeof(counter64)) % 64] = {0};
    alignas(64) stats::operation64 build_index{};
    alignas(64) stats::duration64 seek_event{};

    // Constructor
    SegmentReader() = default;
    // Disable copy/move semantics
    SegmentReader(const SegmentReader&) = delete;
    SegmentReader& operator=(const SegmentReader&) = delete;
    SegmentReader(SegmentReader&&) noexcept = delete;
    SegmentReader& operator=(SegmentReader&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(SegmentReader& other) const noexcept {
        open_segment.copy_to(other.open_segment);
        close_segment.copy_to(other.close_segment);
        verify_segment.copy_to(other.verify_segment);
        total_header_checksum_failures.copy_to(other.total_header_checksum_failures);
        total_block_checksum_failures.copy_to(other.total_block_checksum_failures);
        total_chained_checksum_failures.copy_to(other.total_chained_checksum_failures);
        total_validation_failures.copy_to(other.total_validation_failures);
        build_index.copy_to(other.build_index);
        seek_event.copy_to(other.seek_event);
    }

    // Dump metrics to ostream (human-readable)
    inline void dump(const std::string& label, std::ostream& os) const noexcept {
        os  << "[" << label << " Metrics] Snapshot:\n";
        os << "-----------------------------------------------------------------\n";
        os << " Open segment  : " << open_segment.str(time_unit::milliseconds, time_unit::milliseconds) << "\n";
        os << " Close segment : " << close_segment.str(time_unit::milliseconds, time_unit::milliseconds) << "\n";
        os << " Verify segment: " << verify_segment.str(time_unit::seconds, time_unit::milliseconds) << "\n";
        os << " - Header checksum failures : " << total_header_checksum_failures.load() << "\n";
        os << " - Block checksum failures  : " << total_block_checksum_failures.load() << "\n";
        os << " - Chained checksum failures: " << total_chained_checksum_failures.load() << "\n";
        os << " - Validation failures      : " << total_validation_failures.load() << "\n";
        os << " Build index   : " << build_index.str(time_unit::milliseconds, time_unit::milliseconds) << "\n";
        os << " Seek event    : " << seek_event.str(time_unit::microseconds, time_unit::microseconds) << "\n";
        os << "-----------------------------------------------------------------\n";
    }

    // Metrics collector
    template <typename Collector>
    inline void collect(const std::string& prefix, Collector& collector) const noexcept {
        // Push the current label before serializing
        collector.push_label("subsystem", "wal_recovery_reader");
        // Serialize recovery reader metrics
        open_segment.collect(prefix + "_open_segment", collector);
        close_segment.collect(prefix + "_close_segment", collector);
        verify_segment.collect(prefix + "_verify_segment", collector);
        total_header_checksum_failures.collect(prefix + "_total_header_checksum_failures", "Number of header checksum failures", collector);
        total_block_checksum_failures.collect(prefix + "_total_block_checksum_failures", "Number of block checksum failures", collector);
        total_chained_checksum_failures.collect(prefix + "_total_chained_checksum_failures", "Number of chained checksum failures", collector);
        total_validation_failures.collect(prefix + "_total_validation_failures", "Number of validation failures", collector);
        build_index.collect(prefix + "_build_index", collector);
        seek_event.collect(prefix + "_seek_event", collector);
        // Pop the label after serialization
        collector.pop_label();
    }
};
// Compile-time verification
static_assert(sizeof(SegmentReader) % 64 == 0, "SegmentReader size must be multiple of 64 bytes");
static_assert(alignof(SegmentReader) == 64, "SegmentReader must be aligned to 64 bytes");
static_assert(offsetof(SegmentReader, open_segment) % 64 == 0, "open_segment must start at a cache-line boundary");
static_assert(offsetof(SegmentReader, close_segment) % 64 == 0, "close_segment must start at a cache-line boundary");
static_assert(offsetof(SegmentReader, verify_segment) % 64 == 0, "verify_segment must start at a cache-line boundary");
static_assert(offsetof(SegmentReader, total_header_checksum_failures) % 64 == 0, "total_header_checksum_failures must start at a cache-line boundary");
static_assert(offsetof(SegmentReader, build_index) % 64 == 0, "build_index must start at a cache-line boundary");
static_assert(offsetof(SegmentReader, seek_event) % 64 == 0, "seek_event must start at a cache-line boundary");
// -----------------------------


class SegmentReaderUpdater {
public:
    explicit SegmentReaderUpdater(SegmentReader& metrics) : metrics_(metrics) {}
    // ------------------------------------------------------------------------

    inline void on_open_segment(uint64_t start_ns, Status status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.open_segment.record(start_ns, end_ns, status == Status::OK);
    }

    inline void on_close_segment(uint64_t start_ns, Status status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.close_segment.record(start_ns, end_ns, status == Status::OK);
    }

    inline void on_verify_segment(uint64_t start_ns, Status status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.verify_segment.record(start_ns, end_ns, status == Status::OK);
        if (status != Status::OK) {
            switch (status) {
                case Status::HEADER_CHECKSUM_MISMATCH:
                    metrics_.total_header_checksum_failures.inc();
                    break;
                case Status::BLOCK_CHECKSUM_MISMATCH:
                    metrics_.total_block_checksum_failures.inc();
                    break;
                case Status::CHAINED_CHECKSUM_MISMATCH:
                    metrics_.total_chained_checksum_failures.inc();
                    break;
                case Status::SEGMENT_CORRUPTED:
                case Status::SEGMENT_POSSIBLY_CORRUPTED:
                    metrics_.total_validation_failures.inc();
                    break;
                default:
                    break;
            }
        }
    }

    inline void on_build_index(uint64_t start_ns, Status status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.build_index.record(start_ns, end_ns, status == Status::OK);
    }

    inline void on_seek_event(uint64_t start_ns) const noexcept {
        metrics_.seek_event.record(start_ns, monotonic_clock::instance().now_ns());
    }
    
private:
    SegmentReader& metrics_;
};


} // namespace telemetry
} // namespace recovery
} // namespace wal
} // namespace flashstrike
