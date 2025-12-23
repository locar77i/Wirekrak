#pragma once

#include "lcr/metrics.hpp"
#include "lcr/system/monotonic_clock.hpp"

using namespace lcr::metrics;
using lcr::time_unit;
using lcr::system::monotonic_clock;


namespace flashstrike {
namespace matching_engine {
namespace telemetry {

struct alignas(64) PriceLevelStore {
    // push order 
    alignas(64) stats::duration64 insert_order{};
    alignas(64) latency_histogram insert_order_latency{};
    // modify order price
    alignas(64) stats::duration64 reprice_order{};
    alignas(64) latency_histogram reprice_order_latency{};
    // modify order quantity
    alignas(64) stats::duration64 resize_order{};
    alignas(64) latency_histogram resize_order_latency{};
    // pop order
    alignas(64) stats::duration64 remove_order{};
    alignas(64) latency_histogram remove_order_latency{};
    // recompute global Best Price
    alignas(64) stats::duration64 recompute_global_best{};
    alignas(64) latency_histogram recompute_global_best_latency{};
    // recompute partition best
    alignas(64) stats::duration64 recompute_partition_best{};
    alignas(64) latency_histogram recompute_partition_best_latency{};

    // Constructor
    PriceLevelStore() = default;
    // Disable copy/move semantics
    PriceLevelStore(const PriceLevelStore&) = delete;
    PriceLevelStore& operator=(const PriceLevelStore&) = delete;
    PriceLevelStore(PriceLevelStore&&) noexcept = delete;
    PriceLevelStore& operator=(PriceLevelStore&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(PriceLevelStore& other) const noexcept {
        // push order
        insert_order.copy_to(other.insert_order);
        insert_order_latency.copy_to(other.insert_order_latency);
        // modify order price
        reprice_order.copy_to(other.reprice_order);
        reprice_order_latency.copy_to(other.reprice_order_latency);
        // modify order quantity
        resize_order.copy_to(other.resize_order);
        resize_order_latency.copy_to(other.resize_order_latency);
        // pop order
        remove_order.copy_to(other.remove_order);
        remove_order_latency.copy_to(other.remove_order_latency);
        // recompute global Best Price
        recompute_global_best.copy_to(other.recompute_global_best);
        recompute_global_best_latency.copy_to(other.recompute_global_best_latency);
        // recompute partition best
        recompute_partition_best.copy_to(other.recompute_partition_best);
        recompute_partition_best_latency.copy_to(other.recompute_partition_best_latency);
    }

    // Dump metrics to ostream (human-readable)
    void dump(const std::string& label, std::ostream& os) const noexcept {
        os  << "[" << label << " Metrics] Snapshot:\n";
        os << "-----------------------------------------------------------------\n";
        os << " Insert order: " << insert_order.str(time_unit::seconds, time_unit::microseconds) << "\n";
        os << " -> " << insert_order_latency.compute_percentiles().str(time_unit::microseconds) << "\n";
        os << " --\n";
        os << " Modify order price: " << reprice_order.str(time_unit::milliseconds, time_unit::microseconds) << "\n";
        os << " -> " << reprice_order_latency.compute_percentiles().str(time_unit::microseconds) << "\n";
        os << " --\n";
        os << " Modify order quantity: " << resize_order.str(time_unit::milliseconds, time_unit::microseconds) << "\n";
        os << " -> " << resize_order_latency.compute_percentiles().str(time_unit::microseconds) << "\n";
        os << " --\n";
        os << " Cancel order: " << remove_order.str(time_unit::milliseconds, time_unit::microseconds) << "\n";
        os << " -> " << remove_order_latency.compute_percentiles().str(time_unit::microseconds) << "\n";
        os << " --\n";
        os << " Recompute global best price: " << recompute_global_best.str(time_unit::milliseconds, time_unit::microseconds) << "\n";
        os << " -> " << recompute_global_best_latency.compute_percentiles().str(time_unit::microseconds) << "\n";
        os << " --\n";
        os << " Recompute partition best: " << recompute_partition_best.str(time_unit::milliseconds, time_unit::microseconds) << "\n";
        os << " -> " << recompute_partition_best_latency.compute_percentiles().str(time_unit::microseconds) << "\n";
        os << "-----------------------------------------------------------------\n";
    }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& prefix, Collector& collector) const noexcept {
        collector.push_label("subsystem", "order_book");
        collector.push_label("side", "bids");
        collector.push_label("event", "insert");
        insert_order.collect(prefix + "_insert", collector);
        insert_order_latency.collect(prefix + "_insert_latency", collector);
        collector.pop_label(); // insert event
        collector.push_label("event", "reprice");
        reprice_order.collect(prefix + "_reprice", collector);
        reprice_order_latency.collect(prefix + "_reprice_latency", collector);
        collector.pop_label(); // reprice event
        collector.push_label("event", "resize");
        resize_order.collect(prefix + "_resize", collector);
        resize_order_latency.collect(prefix + "_resize_latency", collector);
        collector.pop_label(); // resize event
        collector.push_label("event", "remove");
        remove_order.collect(prefix + "_remove", collector);
        remove_order_latency.collect(prefix + "_remove_latency", collector);
        collector.pop_label(); // remove event
        collector.push_label("event", "recompute");
        recompute_global_best.collect(prefix + "_recompute_global_best", collector);
        recompute_global_best_latency.collect(prefix + "_recompute_global_best_latency", collector);
        recompute_partition_best.collect(prefix + "_recompute_partition_best", collector);
        recompute_partition_best_latency.collect(prefix + "_recompute_partition_best_latency", collector);
        collector.pop_label(); // recompute event
        collector.pop_label(); // side
        collector.pop_label(); // subsystem
    }
};
// -----------------------------
// Compile-time verification
// -----------------------------
static_assert(sizeof(PriceLevelStore) % 64 == 0, "PriceLevelStore size must be multiple of 64 bytes");
static_assert(alignof(PriceLevelStore) == 64, "PriceLevelStore must be aligned to 64 bytes");
static_assert(offsetof(PriceLevelStore, insert_order) % 64 == 0, "insert_order must start at a cache-line boundary");
static_assert(offsetof(PriceLevelStore, insert_order_latency) % 64 == 0, "insert_order_latency must start at a cache-line boundary");
static_assert(offsetof(PriceLevelStore, reprice_order) % 64 == 0, "reprice_order must start at a cache-line boundary");
static_assert(offsetof(PriceLevelStore, reprice_order_latency) % 64 == 0, "reprice_order_latency must start at a cache-line boundary");
static_assert(offsetof(PriceLevelStore, resize_order) % 64 == 0, "resize_order must start at a cache-line boundary");
static_assert(offsetof(PriceLevelStore, resize_order_latency) % 64 == 0, "resize_order_latency must start at a cache-line boundary");
static_assert(offsetof(PriceLevelStore, remove_order) % 64 == 0, "remove_order must start at a cache-line boundary");
static_assert(offsetof(PriceLevelStore, remove_order_latency) % 64 == 0, "remove_order_latency must start at a cache-line boundary");
static_assert(offsetof(PriceLevelStore, recompute_global_best) % 64 == 0, "recompute_global_best must start at a cache-line boundary");
static_assert(offsetof(PriceLevelStore, recompute_global_best_latency) % 64 == 0, "recompute_global_best_latency must start at a cache-line boundary");
static_assert(offsetof(PriceLevelStore, recompute_partition_best) % 64 == 0, "recompute_partition_best must start at a cache-line boundary");
static_assert(offsetof(PriceLevelStore, recompute_partition_best_latency) % 64 == 0, "recompute_partition_best_latency must start at a cache-line boundary");
// -----------------------------


class PriceLevelStoreUpdater {
public:
    explicit PriceLevelStoreUpdater(PriceLevelStore& asks_metrics, PriceLevelStore& bids_metrics)
        : asks_metrics_(asks_metrics)
        , bids_metrics_(bids_metrics)
    {}
    // ------------------------------------------------------------------------

    // Accessors
    template<Side SIDE>
    inline PriceLevelStore& get_metrics() const noexcept {
        if constexpr (SIDE == Side::BID) return bids_metrics_;
        else return asks_metrics_;
    }

    template<Side SIDE>
    inline void on_insert_order(uint64_t start_ns) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        get_metrics<SIDE>().insert_order.record(start_ns, end_ns);
        get_metrics<SIDE>().insert_order_latency.record(start_ns, end_ns);
    }

    template<Side SIDE>
    inline void on_reprice_order(uint64_t start_ns) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        get_metrics<SIDE>().reprice_order.record(start_ns, end_ns);
        get_metrics<SIDE>().reprice_order_latency.record(start_ns, end_ns);
    }

    template<Side SIDE>
    inline void on_resize_order(uint64_t start_ns) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        get_metrics<SIDE>().resize_order.record(start_ns, end_ns);
        get_metrics<SIDE>().resize_order_latency.record(start_ns, end_ns);
    }

    template<Side SIDE>
    inline void on_remove_order(uint64_t start_ns) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        get_metrics<SIDE>().remove_order.record(start_ns, end_ns);
        get_metrics<SIDE>().remove_order_latency.record(start_ns, end_ns);
    }

    template<Side SIDE>
    inline void on_recompute_global_best(uint64_t start_ns) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        get_metrics<SIDE>().recompute_global_best.record(start_ns, end_ns);
        get_metrics<SIDE>().recompute_global_best_latency.record(start_ns, end_ns);
    }

    template<Side SIDE>
    inline void on_recompute_partition_best(uint64_t start_ns) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        get_metrics<SIDE>().recompute_partition_best.record(start_ns, end_ns);
        get_metrics<SIDE>().recompute_partition_best_latency.record(start_ns, end_ns);
    }

    void dump(const std::string& label, std::ostream& os) const noexcept {
        asks_metrics_.dump(label + " (Asks)", os);
        bids_metrics_.dump(label + " (Bids)", os);
    }

private:
    PriceLevelStore& asks_metrics_;
    PriceLevelStore& bids_metrics_;
};


} // namespace telemetry
} // namespace matching_engine
} // namespace flashstrike
