#pragma once

#include "flashstrike/matching_engine/telemetry.hpp"
#include "flashstrike/wal/recovery/telemetry.hpp"
#include "flashstrike/wal/recorder/telemetry.hpp"


namespace flashstrike {
namespace instrument {
namespace telemetry {


struct Engine {
    matching_engine::Telemetry matching_engine;
    wal::recorder::Telemetry recorder;
    wal::recovery::Telemetry recovery;

    // Constructor
    Engine() = default;
    // Disable copy/move semantics
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) noexcept = delete;
    Engine& operator=(Engine&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(Engine& other) const noexcept {
        matching_engine.copy_to(other.matching_engine);
        recorder.copy_to(other.recorder);
    }

    // Helpers -------------------------------------------------------------------
    void dump(const std::string& label, std::ostream& os) const noexcept {
        os << "*****************************************************************\n";
        os  << "[" << label << "] Instrument Metrics:\n";
        os << "*****************************************************************\n";
        matching_engine.dump(label, os);
        recorder.dump(label, os);
        recovery.dump(label, os);
    }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& pair, Collector& collector) const noexcept {
        // Push the current label before serializing
        collector.push_label("pair", pair);
        // Serialize subsystems metrics
        matching_engine.collect(collector);
        recorder.collect(collector);
        recovery.collect(collector);
        // Pop the label after serialization
        collector.pop_label();
    }
};

} // namespace telemetry
} // namespace instrument
} // namespace flashstrike
