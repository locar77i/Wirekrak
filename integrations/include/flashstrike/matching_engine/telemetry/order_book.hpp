#pragma once

#include "flashstrike/matching_engine/telemetry/price_level_store.hpp"
#include "lcr/metrics.hpp"

using namespace lcr::metrics;


namespace flashstrike {
namespace matching_engine {
namespace telemetry {

// ---------------------------------------------------------------------------
//  matching_engine::OrderBook - Ultra-low-overhead telemetry for production HFT systems
// ---------------------------------------------------------------------------
struct OrderBook {
    telemetry::PriceLevelStore pls_asks_metrics;
    telemetry::PriceLevelStore pls_bids_metrics;

    // Constructor
    OrderBook() = default;
    // Disable copy/move semantics
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) noexcept = delete;
    OrderBook& operator=(OrderBook&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(OrderBook& other) const noexcept {
        pls_asks_metrics.copy_to(other.pls_asks_metrics);
        pls_bids_metrics.copy_to(other.pls_bids_metrics);
    }

    // Helpers -------------------------------------------------------------------
    void dump(const std::string& label, std::ostream& os) const noexcept  {
        os << "-----------------------------------------------------------------\n";
        os  << "[" << label << "] Snapshot:\n";
        os << "-----------------------------------------------------------------\n";
        pls_asks_metrics.dump("Price Levels - Asks", os);
        pls_bids_metrics.dump("Price Levels - Bids", os);
    }

    // Metrics collector
    template <typename Collector>
    void collect(Collector& collector) const noexcept {
        // Push the current label before serializing
        collector.push_label("system", "matching_engine");
        // Serialize matching engine metrics
        std::string prefix = "mc_me";
        pls_asks_metrics.collect(prefix + "_asks", collector);
        pls_bids_metrics.collect(prefix + "_bids", collector);
        // Pop the label after serialization
        collector.pop_label();
    }
};

} // namespace telemetry
} // namespace matching_engine
} // namespace flashstrike
