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

struct alignas(64) SegmentWriter {
    alignas(64) stats::operation64 open_new_segment{};
    alignas(64) stats::operation64 open_existing_segment{};
    alignas(64) stats::operation64 close_segment{};
    alignas(64) stats::duration64 write_block{};
    alignas(64) counter64 total_header_checksum_failures{};
    counter64 total_block_checksum_failures{};
    counter64 total_chained_checksum_failures{};
    counter64 total_validation_failures{};
    char pad_[64 - (4 * sizeof(counter64)) % 64] = {0};

    // Constructor
    SegmentWriter() = default;
    // Disable copy/move semantics
    SegmentWriter(const SegmentWriter&) = delete;
    SegmentWriter& operator=(const SegmentWriter&) = delete;
    SegmentWriter(SegmentWriter&&) noexcept = delete;
    SegmentWriter& operator=(SegmentWriter&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(SegmentWriter& other) const noexcept {
        open_new_segment.copy_to(other.open_new_segment);
        open_existing_segment.copy_to(other.open_existing_segment);
        close_segment.copy_to(other.close_segment);
        write_block.copy_to(other.write_block);
        total_header_checksum_failures.copy_to(other.total_header_checksum_failures);
        total_block_checksum_failures.copy_to(other.total_block_checksum_failures);
        total_chained_checksum_failures.copy_to(other.total_chained_checksum_failures);
        total_validation_failures.copy_to(other.total_validation_failures);
    }

    // Dump metrics to ostream (human-readable)
    inline void dump(const std::string& label, std::ostream& os) const noexcept {
        os  << "[" << label << " Metrics] Snapshot:\n";
        os << "-----------------------------------------------------------------\n";
        os << " Open new segment     : " << open_new_segment.str(time_unit::milliseconds, time_unit::milliseconds) << "\n";
        os << " Open existing segment: " << open_existing_segment.str(time_unit::milliseconds, time_unit::milliseconds) << "\n";
        os << " Close segment        : " << close_segment.str(time_unit::milliseconds, time_unit::milliseconds) << "\n";
        os << " Verify segment:\n";
        os << " - Header checksum failures : " << total_header_checksum_failures.load() << "\n";
        os << " - Block checksum failures  : " << total_block_checksum_failures.load() << "\n";
        os << " - Chained checksum failures: " << total_chained_checksum_failures.load() << "\n";
        os << " - Validation failures      : " << total_validation_failures.load() << "\n";
        os << " Write block: " << write_block.str(time_unit::milliseconds, time_unit::microseconds) << "\n";
        os << "-----------------------------------------------------------------\n";
    }

    // Metrics collector
    template <typename Collector>
    inline void collect(const std::string& prefix, Collector& collector) const noexcept {
        // Push the current label before serializing
        collector.push_label("subsystem", "wal_segment_writer");
        // Serialize segment metrics
        open_new_segment.collect(prefix + "_open_new_segment", collector);
        open_existing_segment.collect(prefix + "_open_existing_segment", collector);
        close_segment.collect(prefix + "_close_segment", collector);
        write_block.collect(prefix + "_write_block_ns", collector);
        total_header_checksum_failures.collect(prefix + "_total_header_checksum_failures", "Number of header checksum failures", collector);
        total_block_checksum_failures.collect(prefix + "_total_block_checksum_failures", "Number of block checksum failures", collector);
        total_chained_checksum_failures.collect(prefix + "_total_chained_checksum_failures", "Number of chained checksum failures", collector);
        total_validation_failures.collect(prefix + "_total_validation_failures", "Number of validation failures", collector);
        // Pop the label after serialization
        collector.pop_label();
    }
};
// Compile-time verification
static_assert(sizeof(SegmentWriter) % 64 == 0, "SegmentWriter size must be multiple of 64 bytes");
static_assert(alignof(SegmentWriter) == 64, "SegmentWriter must be aligned to 64 bytes");
static_assert(offsetof(SegmentWriter, open_new_segment) % 64 == 0, "open_new_segment must start at a cache-line boundary");
static_assert(offsetof(SegmentWriter, open_existing_segment) % 64 == 0, "open_existing_segment must start at a cache-line boundary");
static_assert(offsetof(SegmentWriter, close_segment) % 64 == 0, "close_segment must start at a cache-line boundary");
static_assert(offsetof(SegmentWriter, write_block) % 64 == 0, "write_block must start at a cache-line boundary");
static_assert(offsetof(SegmentWriter, total_header_checksum_failures) % 64 == 0, "total_header_checksum_failures must start at a cache-line boundary");

class SegmentWriterUpdater {
public:
    explicit SegmentWriterUpdater(SegmentWriter& metrics) : metrics_(metrics) {}

    inline void on_open_new_segment(uint64_t start_ns, Status status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.open_new_segment.record(start_ns, end_ns, status == Status::OK);
    }

    inline void on_open_existing_segment(uint64_t start_ns, Status status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.open_existing_segment.record(start_ns, end_ns, status == Status::OK);
    }

    inline void on_close_segment(uint64_t start_ns, Status status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.close_segment.record(start_ns, end_ns, status == Status::OK);
    }

    inline void on_write_block_(uint64_t start_ns) const noexcept {
        metrics_.write_block.record(start_ns, monotonic_clock::instance().now_ns());
    }

    inline void on_integrity_failure(Status status) const noexcept {
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

private:
    SegmentWriter& metrics_;
};


} // namespace telemetry
} // namespace recorder
} // namespace wal
} // namespace flashstrike
