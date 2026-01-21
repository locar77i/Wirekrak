#pragma once

#include <string>
#include <ostream>

#include "flashstrike/wal/recovery/telemetry/segment_reader.hpp"
#include "flashstrike/wal/recovery/telemetry/worker/segment_preloader.hpp"
#include "flashstrike/wal/recovery/telemetry/manager.hpp"


namespace flashstrike {
namespace wal {
namespace recovery {

struct Telemetry {
    telemetry::SegmentReader segment_reader_metrics{};
    telemetry::worker::SegmentPreloader segment_preloader_metrics{};
    telemetry::Manager manager_metrics{};

    // Constructor
    Telemetry() = default;
    // Disable copy/move semantics
    Telemetry(const Telemetry&) = delete;
    Telemetry& operator=(const Telemetry&) = delete;
    Telemetry(Telemetry&&) noexcept = delete;
    Telemetry& operator=(Telemetry&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(Telemetry& other) const noexcept {
        segment_reader_metrics.copy_to(other.segment_reader_metrics);
        segment_preloader_metrics.copy_to(other.segment_preloader_metrics);
        manager_metrics.copy_to(other.manager_metrics);
    }

    // Helpers -------------------------------------------------------------------
    inline void dump(const std::string& label, std::ostream& os) const noexcept {
        os << "-----------------------------------------------------------------\n";
        os  << "[" << label << "] WAL Recovery Telemetry:\n";
        os << "-----------------------------------------------------------------\n";
    #ifdef ENABLE_FS1_METRICS
        segment_reader_metrics.dump("Segment Reader", os);
        segment_preloader_metrics.dump("Segment Preloader", os);
        manager_metrics.dump("Manager", os);
    #endif
    }

    // Telemetry collector
    template <typename Collector>
    inline void collect(Collector& collector) const noexcept {
        // Push the current label before serializing
        collector.push_label("system", "wal_recovery");
        // Serialize WAL recovery metrics
        std::string prefix = "ie_wal_recovery_";
        segment_reader_metrics.collect(prefix + "segment_reader_", collector);
        segment_preloader_metrics.collect(prefix + "segment_preloader_", collector);
        manager_metrics.collect(prefix + "manager_", collector);
        // Pop the label after serialization
        collector.pop_label();
    }
};

} // namespace recovery
} // namespace wal
} // namespace flashstrike
