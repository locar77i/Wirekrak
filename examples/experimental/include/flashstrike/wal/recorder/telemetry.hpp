#pragma once

#include <string>
#include <ostream>

#include "flashstrike/wal/recorder/telemetry/meta.hpp"
#include "flashstrike/wal/recorder/telemetry/segment_writer.hpp"
#include "flashstrike/wal/recorder/telemetry/worker/segment_preparer.hpp"
#include "flashstrike/wal/recorder/telemetry/worker/segment_maintainer.hpp"
#include "flashstrike/wal/recorder/telemetry/manager.hpp"


namespace flashstrike {
namespace wal {
namespace recorder {

struct Telemetry {
    telemetry::MetaStore meta_store_metrics{};
    telemetry::SegmentWriter segment_writer_metrics{};
    telemetry::worker::SegmentPreparer segment_preparer_metrics{};
    telemetry::worker::SegmentMaintainer segment_maintainer_metrics{};
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
        meta_store_metrics.copy_to(other.meta_store_metrics);
        segment_writer_metrics.copy_to(other.segment_writer_metrics);
        segment_preparer_metrics.copy_to(other.segment_preparer_metrics);
        segment_maintainer_metrics.copy_to(other.segment_maintainer_metrics);
        manager_metrics.copy_to(other.manager_metrics);
    }

    // Helpers -------------------------------------------------------------------
    inline void dump(const std::string& label, std::ostream& os) const noexcept {
        os << "-----------------------------------------------------------------\n";
        os  << "[" << label << "] WAL Recorder Metrics:\n";
        os << "-----------------------------------------------------------------\n";
    #ifdef ENABLE_FS1_METRICS
        meta_store_metrics.dump("Meta Store", os);
        segment_writer_metrics.dump("Segment Writer", os);
        segment_preparer_metrics.dump("Segment Preparer", os);
        segment_maintainer_metrics.dump("Segment Maintainer", os);
        manager_metrics.dump("Manager", os);
    #endif
    }

    // Metrics collector
    template <typename Collector>
    inline void collect(Collector& collector) const noexcept {
        // Push the current label before serializing
        collector.push_label("system", "wal_recorder");
        // Serialize WAL writer metrics
        std::string prefix = "ie_wal_recorder_";
        meta_store_metrics.collect(prefix + "_meta_store", collector);
        segment_writer_metrics.collect(prefix + "_segment_writer", collector);
        segment_preparer_metrics.collect(prefix + "_segment_preparer", collector);
        segment_maintainer_metrics.collect(prefix + "_segment_maintainer", collector);
        manager_metrics.collect(prefix + "_manager", collector);
        // Pop the label after serialization
        collector.pop_label();
    }
};


} // namespace recorder
} // namespace wal
} // namespace flashstrike
