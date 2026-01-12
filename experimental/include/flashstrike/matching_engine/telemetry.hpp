#pragma once

#include "flashstrike/matching_engine/telemetry/init.hpp"
#include "flashstrike/matching_engine/telemetry/manager.hpp"
#include "flashstrike/matching_engine/telemetry/price_level_store.hpp"
#include "flashstrike/matching_engine/telemetry/low_level.hpp"


namespace flashstrike {
namespace matching_engine {

// ---------------------------------------------------------------------------
//  matching_engine::Telemetry - Ultra-low-overhead telemetry for production HFT systems
// ---------------------------------------------------------------------------
struct Telemetry {
    telemetry::Init init_metrics;
    telemetry::Manager manager_metrics;
    telemetry::PriceLevelStore pls_asks_metrics;
    telemetry::PriceLevelStore pls_bids_metrics;
    telemetry::LowLevel low_level_metrics;

    // Constructor
    Telemetry() = default;
    // Disable copy/move semantics
    Telemetry(const Telemetry&) = delete;
    Telemetry& operator=(const Telemetry&) = delete;
    Telemetry(Telemetry&&) noexcept = delete;
    Telemetry& operator=(Telemetry&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(Telemetry& other) const noexcept {
        init_metrics.copy_to(other.init_metrics);
//#ifdef ENABLE_FS_METRICS
        manager_metrics.copy_to(other.manager_metrics);
//#endif
#ifdef ENABLE_FS2_METRICS
        pls_asks_metrics.copy_to(other.pls_asks_metrics);
        pls_bids_metrics.copy_to(other.pls_bids_metrics);
#endif
#ifdef ENABLE_FS3_METRICS
        low_level_metrics.copy_to(other.low_level_metrics);
#endif
    }

    // Helpers -------------------------------------------------------------------
    void dump(const std::string& label, std::ostream& os) const noexcept  {
        os << "-----------------------------------------------------------------\n";
        os  << "[" << label << "] Matching Engine Metrics:\n";
        os << "-----------------------------------------------------------------\n";
        init_metrics.dump("Configuration", os);
    //#ifdef ENABLE_FS_METRICS
        manager_metrics.dump("Matching Engine", os);
    //#endif
    #ifdef ENABLE_FS2_METRICS
        pls_asks_metrics.dump("Price Levels - Asks", os);
        pls_bids_metrics.dump("Price Levels - Bids", os);
    #endif
    #ifdef ENABLE_FS3_METRICS
        low_level_metrics.dump("Core", os);
    #endif
    }

    // Metrics collector
    template <typename Collector>
    void collect(Collector& collector) const noexcept {
        // Push the current label before serializing
        collector.push_label("system", "matching_engine");
        // Serialize matching engine metrics
        std::string prefix = "mc_me";
        init_metrics.collect(prefix + "_init", collector);
        manager_metrics.collect(prefix, collector);
#ifdef ENABLE_FS2_METRICS
        pls_asks_metrics.collect(prefix + "_asks", collector);
        pls_bids_metrics.collect(prefix + "_bids", collector);
#endif
#ifdef ENABLE_FS3_METRICS
        low_level_metrics.collect(prefix + "_core", collector);
#endif
        // Pop the label after serialization
        collector.pop_label();
    }
};


} // namespace matching_engine
} // namespace flashstrike
