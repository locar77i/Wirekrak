#pragma once

#include "flashstrike/types.hpp"
#include "lcr/metrics.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/format.hpp"

using namespace lcr::metrics;
using lcr::time_unit;
using lcr::system::monotonic_clock;


namespace flashstrike {
namespace matching_engine {
namespace telemetry {

struct alignas(64) Manager {
    alignas(64) gauge64 order_pool_size{0};
    gauge64 order_id_map_size{0};
    gauge64 partition_pool_size{0};
    gauge64 trades_ring_size{0};
    // process order 
    alignas(64) stats::operation64 process{};
    alignas(64) latency_histogram process_latency{};
    alignas(64) stats::operation64 process_on_fly{};
    alignas(64) latency_histogram process_on_fly_latency{};
    alignas(64) stats::operation64 process_resting{};
    alignas(64) latency_histogram process_resting_latency{};
    // modify order price
    alignas(64) stats::operation64 modify_price{};
    counter64 modify_price_not_found_total{};
    counter64 modify_price_rejected_total{};
    alignas(64) latency_histogram modify_price_latency{};
    // modify order quantity
    alignas(64) stats::operation64 modify_qty{};
    counter64 modify_qty_not_found_total{};
    counter64 modify_qty_rejected_total{};
    alignas(64) latency_histogram modify_qty_latency{};
    // cancel order
    alignas(64) stats::operation64 cancel{};
    counter64 cancel_not_found_total{};
    alignas(64) latency_histogram cancel_latency{};
    // matching orders
    alignas(64) stats::duration64 match{};
    alignas(64) latency_histogram match_latency{};
    alignas(64) stats::size32 match_order_trades{};
    alignas(64) counter64 full_match_total{};
    counter64 partial_match_total{};
    counter64 no_match_total{};
    counter64 removed_on_match_total{};

    // Constructor
    Manager() = default;
    // Disable copy/move semantics
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) noexcept = delete;
    Manager& operator=(Manager&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(Manager& other) const noexcept {
        order_pool_size.copy_to(other.order_pool_size);
        order_id_map_size.copy_to(other.order_id_map_size);
        partition_pool_size.copy_to(other.partition_pool_size);
        trades_ring_size.copy_to(other.trades_ring_size);
        // process order
        process.copy_to(other.process);
#ifdef ENABLE_FS1_METRICS
        process_latency.copy_to(other.process_latency);
        process_on_fly.copy_to(other.process_on_fly);
        process_resting.copy_to(other.process_resting);
#endif
#ifdef ENABLE_FS2_METRICS
        process_on_fly_latency.copy_to(other.process_on_fly_latency);
        process_resting_latency.copy_to(other.process_resting_latency);
#endif
        // modify order price
        modify_price.copy_to(other.modify_price);
        modify_price_not_found_total.copy_to(other.modify_price_not_found_total);
        modify_price_rejected_total.copy_to(other.modify_price_rejected_total);
#ifdef ENABLE_FS1_METRICS
        modify_price_latency.copy_to(other.modify_price_latency);
#endif
        // modify order quantity
        modify_qty.copy_to(other.modify_qty);
        modify_qty_not_found_total.copy_to(other.modify_qty_not_found_total);
        modify_qty_rejected_total.copy_to(other.modify_qty_rejected_total);
#ifdef ENABLE_FS1_METRICS
        modify_qty_latency.copy_to(other.modify_qty_latency);
#endif
        // cancel order
        cancel.copy_to(other.cancel);
        cancel_not_found_total.copy_to(other.cancel_not_found_total);
#ifdef ENABLE_FS1_METRICS
        cancel_latency.copy_to(other.cancel_latency);
#endif
        // matching orders
#ifdef ENABLE_FS2_METRICS
        match.copy_to(other.match);
        match_latency.copy_to(other.match_latency);
        match_order_trades.copy_to(other.match_order_trades);
        full_match_total.copy_to(other.full_match_total);
        partial_match_total.copy_to(other.partial_match_total);
        no_match_total.copy_to(other.no_match_total);
        removed_on_match_total.copy_to(other.removed_on_match_total);
#endif // #ifdef ENABLE_FS2_METRICS
    }

    // Dump metrics to ostream (human-readable)
    void dump(const std::string& label, std::ostream& os) const noexcept {
        // Calculate 
        std::uint64_t total_samples = process.samples() + modify_price.samples() +  modify_qty.samples() + cancel.samples();
        std::uint64_t total_ns = process.total_ns() + modify_price.total_ns() + modify_qty.total_ns() + cancel.total_ns();
        double seconds = static_cast<double>(total_ns) / 1'000'000'000.0;
        double rps = static_cast<double>(total_samples) / seconds;
        // Print
        os  << "[" << label << " Metrics] Snapshot:\n";
        os << "-----------------------------------------------------------------\n";
        os << " Order pool size     : " << order_pool_size.load() << "\n";
        os << " Order id map size   : " << order_id_map_size.load() << "\n";
        os << " Partition pool size : " << partition_pool_size.load() << "\n";
        os << " Trades ring size    : " << trades_ring_size.load() << "\n";
        os << "-----------------------------------------------------------------\n";
        os << " Request processing  : " << lcr::format_throughput(rps, "req/s") << "\n";
        os << " --\n";
        os << " Process order        : " << process.str(time_unit::seconds, time_unit::microseconds) << "\n";
        os << " -> " << process_latency.compute_percentiles().str(time_unit::microseconds) << "\n";
#ifdef ENABLE_FS1_METRICS
        os << " Process on-fly order : " << process_on_fly.str(time_unit::seconds, time_unit::microseconds) << "\n";
#endif
#ifdef ENABLE_FS2_METRICS
        os << " -> " << process_on_fly_latency.compute_percentiles().str(time_unit::microseconds) << "\n";
#endif
#ifdef ENABLE_FS1_METRICS
        os << " Process resting order: " << process_resting.str(time_unit::seconds, time_unit::microseconds) << "\n";
#endif
#ifdef ENABLE_FS2_METRICS
        os << " -> " << process_resting_latency.compute_percentiles().str(time_unit::microseconds) << "\n";
#endif
        os << " --\n";
        os << " Modify order price: " << modify_price.str(time_unit::milliseconds, time_unit::microseconds) << "\n";
        os << " -> " << modify_price_latency.compute_percentiles().str(time_unit::microseconds) << "\n";
        os << " - Not found: " << modify_price_not_found_total.load() << "\n";
        os << " - Rejected : " << modify_price_rejected_total.load() << "\n";
        os << " --\n";
        os << " Modify order quantity: " << modify_qty.str(time_unit::milliseconds, time_unit::microseconds) << "\n";
        os << " -> " << modify_qty_latency.compute_percentiles().str(time_unit::microseconds) << "\n";
        os << " - Not found: " << modify_qty_not_found_total.load() << "\n";
        os << " - Rejected : " << modify_qty_rejected_total.load() << "\n";
        os << " --\n";
        os << " Cancel order: " << cancel.str(time_unit::milliseconds, time_unit::microseconds) << "\n";
        os << " -> " << cancel_latency.compute_percentiles().str(time_unit::microseconds) << "\n";
        os << " - Not found: " << cancel_not_found_total.load() << "\n";
#ifdef ENABLE_FS2_METRICS
        os << " --\n";
        os << " Match order     : " << match.str(time_unit::milliseconds, time_unit::microseconds) << "\n";
        os << " -> " << match_latency.compute_percentiles().str(time_unit::microseconds) << "\n";
        os << " - Trades        : " << match_order_trades.str() << "\n";
        os << " - Full fills    : " << full_match_total.load() << "\n";
        os << " - Partial fills : " << partial_match_total.load() << "\n";
        os << " - Not matched   : " << no_match_total.load() << "\n";
        os << " - Orders removed: " << removed_on_match_total.load() << "\n";
#endif // #ifdef ENABLE_FS2_METRICS
        os << "-----------------------------------------------------------------\n";
    }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& prefix, Collector& collector) const noexcept {
        order_pool_size.collect(prefix + "_order_pool_size", "Order Pool current size", collector);
        order_id_map_size.collect(prefix + "_order_id_map_size", "Order ID Map current size", collector);
        partition_pool_size.collect(prefix + "_partition_pool_size", "Partition Pool current size", collector);
        trades_ring_size.collect(prefix + "_trades_ring_size", "Trades ring buffer current size", collector);
        collector.push_label("direction", "input");
        // process order
        collector.push_label("event", "process");
        process.collect(prefix + "_process", collector);
#ifdef ENABLE_FS1_METRICS
        process_latency.collect(prefix + "_process_latency", collector);
        process_on_fly.collect(prefix + "_process_on_fly", collector);
        process_resting.collect(prefix + "_process_resting", collector);
#endif
#ifdef ENABLE_FS2_METRICS
        process_on_fly_latency.collect(prefix + "_process_on_fly_latency", collector);
        process_resting_latency.collect(prefix + "_process_resting_latency", collector);
#endif
        collector.pop_label(); // process event
        // modify order price
        collector.push_label("event", "modify_price");
        modify_price.collect(prefix + "_modify_price", collector);
        modify_price_not_found_total.collect(prefix + "_modify_price_not_found_total", "Number of not found orders when modifying price", collector);
        modify_price_rejected_total.collect(prefix + "_modify_price_rejected_total", "Number of rejected orders when modifying price", collector);
#ifdef ENABLE_FS1_METRICS
        modify_price_latency.collect(prefix + "_modify_price_latency", collector);
#endif
        collector.pop_label(); // modify_price event
        // modify order quantity
        collector.push_label("event", "modify_qty");
        modify_qty.collect(prefix + "_modify_qty", collector);
        modify_qty_not_found_total.collect(prefix + "_modify_qty_not_found_total", "Number of not found orders when modifying quantity", collector);
        modify_qty_rejected_total.collect(prefix + "_modify_qty_rejected_total", "Number of rejected orders when modifying quantity", collector);
#ifdef ENABLE_FS1_METRICS
        modify_qty_latency.collect(prefix + "_modify_qty_latency", collector);
#endif
        collector.pop_label(); // modify_qty event
        // cancel order
        collector.push_label("event", "cancel");
        cancel.collect(prefix + "_cancel", collector);
        cancel_not_found_total.collect(prefix + "_cancel_not_found_total", "Number of not found orders when canceling", collector);
#ifdef ENABLE_FS1_METRICS
        cancel_latency.collect(prefix + "_cancel_latency", collector);
#endif
        collector.pop_label(); // cancel event
        collector.pop_label(); // input direction
        // matching orders
#ifdef ENABLE_FS2_METRICS
        collector.push_label("direction", "output");
        collector.push_label("event", "match");
        match.collect(prefix + "_match", collector);
        match_latency.collect(prefix + "_match_latency", collector);
        match_order_trades.collect(prefix + "_match_order_trades", collector);
        full_match_total.collect(prefix + "_full_match_total", "Full fills count during matching", collector);
        partial_match_total.collect(prefix + "_partial_match_total", "Partial fills count during matching", collector);
        no_match_total.collect(prefix + "_no_match_total", "No match count during matching", collector);
        removed_on_match_total.collect(prefix + "_removed_on_match_total", "Removed orders count during matching", collector);
        collector.pop_label(); // match event
        collector.pop_label(); // output direction
#endif // #ifdef ENABLE_FS2_METRICS
    }
};
// -----------------------------
// Compile-time verification
// -----------------------------
static_assert(sizeof(Manager) % 64 == 0, "Manager size must be multiple of 64 bytes");
static_assert(alignof(Manager) == 64, "Manager must be aligned to 64 bytes");
static_assert(offsetof(Manager, order_pool_size) % 64 == 0, "order_pool_size must start at a cache-line boundary");
static_assert(offsetof(Manager, process) % 64 == 0, "process must start at a cache-line boundary");
static_assert(offsetof(Manager, process_latency) % 64 == 0, "process_latency must start at a cache-line boundary");
static_assert(offsetof(Manager, process_on_fly) % 64 == 0, "process_on_fly must start at a cache-line boundary");
static_assert(offsetof(Manager, process_on_fly_latency) % 64 == 0, "process_on_fly_latency must start at a cache-line boundary");
static_assert(offsetof(Manager, process_resting) % 64 == 0, "process_resting must start at a cache-line boundary");
static_assert(offsetof(Manager, process_resting_latency) % 64 == 0, "process_resting_latency must start at a cache-line boundary");
static_assert(offsetof(Manager, modify_price) % 64 == 0, "modify_price must start at a cache-line boundary");
static_assert(offsetof(Manager, modify_price_latency) % 64 == 0, "modify_price_latency must start at a cache-line boundary");
static_assert(offsetof(Manager, modify_qty) % 64 == 0, "modify_qty must start at a cache-line boundary");
static_assert(offsetof(Manager, modify_qty_latency) % 64 == 0, "modify_qty_latency must start at a cache-line boundary");
static_assert(offsetof(Manager, cancel) % 64 == 0, "cancel must start at a cache-line boundary");
static_assert(offsetof(Manager, cancel_latency) % 64 == 0, "cancel_latency must start at a cache-line boundary");
static_assert(offsetof(Manager, match) % 64 == 0, "match must start at a cache-line boundary");
static_assert(offsetof(Manager, match_latency) % 64 == 0, "match_latency must start at a cache-line boundary");
static_assert(offsetof(Manager, match_order_trades) % 64 == 0, "match_order_trades must start at a cache-line boundary");
static_assert(offsetof(Manager, full_match_total) % 64 == 0, "full_match_total must start at a cache-line boundary");
// -----------------------------

class ManagerUpdater {
public:
    explicit ManagerUpdater(Manager& metrics) : metrics_(metrics) {}
    // ------------------------------------------------------------------------

    inline void on_every_n_requests(std::uint64_t order_pool_size, std::uint64_t order_id_map_size, std::uint64_t partition_pool_size, std::uint64_t trades_ring_size) const noexcept {
        metrics_.order_pool_size.store(order_pool_size);
        metrics_.order_id_map_size.store(order_id_map_size);
        metrics_.partition_pool_size.store(partition_pool_size);
        metrics_.trades_ring_size.store(trades_ring_size);
    }

    // Function to check if the order insert was successful
    inline bool is_insert_successful(OperationStatus status) const noexcept {
        return status == OperationStatus::SUCCESS ||
            status == OperationStatus::FULL_FILL ||
            status == OperationStatus::PARTIAL_FILL ||
            status == OperationStatus::NO_MATCH;
    }

    inline void on_process_on_fly_order(uint64_t start_ns, OperationStatus status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        bool success = is_insert_successful(status);
        metrics_.process.record(start_ns, end_ns, success);
#ifdef ENABLE_FS1_METRICS
        metrics_.process_latency.record(start_ns, end_ns);
        metrics_.process_on_fly.record(start_ns, end_ns, success);
#endif
#ifdef ENABLE_FS2_METRICS
        metrics_.process_on_fly_latency.record(start_ns, end_ns);
#endif
    }

    inline void on_process_resting_order(uint64_t start_ns, OperationStatus status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        bool success = is_insert_successful(status);
        metrics_.process.record(start_ns, end_ns, success);
#ifdef ENABLE_FS1_METRICS
        metrics_.process_latency.record(start_ns, end_ns);
        metrics_.process_resting.record(start_ns, end_ns, success);
#endif
#ifdef ENABLE_FS2_METRICS
        metrics_.process_resting_latency.record(start_ns, end_ns);
#endif
    }

    // Function to check if the order modify was successful
    inline bool is_modify_successful(OperationStatus status) const noexcept {
        return status == OperationStatus::SUCCESS ||
            status == OperationStatus::UNCHANGED ||
            status == OperationStatus::FULL_FILL ||
            status == OperationStatus::PARTIAL_FILL;
    }

    inline void on_modify_order_price(uint64_t start_ns, OperationStatus status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        bool success = is_modify_successful(status);
        metrics_.modify_price.record(start_ns, end_ns, success);
#ifdef ENABLE_FS1_METRICS
        metrics_.modify_price_latency.record(start_ns, end_ns);
#endif
        switch (status) {
            case OperationStatus::NOT_FOUND:
                metrics_.modify_price_not_found_total.inc();
                break;
            case OperationStatus::REJECTED:
                metrics_.modify_price_rejected_total.inc();
                break;
            default:
                break;
        }
    }

    inline void on_modify_order_quantity(uint64_t start_ns, OperationStatus status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        bool success = is_modify_successful(status);
        metrics_.modify_qty.record(start_ns, end_ns, success);
#ifdef ENABLE_FS1_METRICS
        metrics_.modify_qty_latency.record(start_ns, end_ns);
#endif
        switch (status) {
            case OperationStatus::NOT_FOUND:
                metrics_.modify_qty_not_found_total.inc();
                break;
            case OperationStatus::REJECTED:
                metrics_.modify_qty_rejected_total.inc();
                break;
            default:
                break;
        }
    }

    inline void on_cancel_order(uint64_t start_ns, OperationStatus status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        bool success = (status == OperationStatus::SUCCESS);
        metrics_.cancel.record(start_ns, end_ns, success);
#ifdef ENABLE_FS1_METRICS
        metrics_.cancel_latency.record(start_ns, end_ns);
#endif
        switch (status) {
            case OperationStatus::NOT_FOUND:
                metrics_.cancel_not_found_total.inc();
                break;
            default:
                break;
        }
    }

    inline void on_match_order(uint64_t start_ns, Trades trades, OperationStatus status) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.match.record(start_ns, end_ns);
        metrics_.match_latency.record(start_ns, end_ns);
        metrics_.match_order_trades.set(trades);
        switch (status) {
            case OperationStatus::FULL_FILL:
                metrics_.full_match_total.inc();
                break;
            case OperationStatus::PARTIAL_FILL:
                metrics_.partial_match_total.inc();
                break;
            case OperationStatus::NO_MATCH:
                metrics_.no_match_total.inc();
                break;
            default:
                break;
        }
    }

    inline void on_remove_order_after_match() const noexcept {
        metrics_.removed_on_match_total.inc();
    }

    void dump(const std::string& label, std::ostream& os) const noexcept {
        metrics_.dump(label, os);
    }

private:
    Manager& metrics_;
};



} // namespace telemetry
} // namespace matching_engine
} // namespace flashstrike
