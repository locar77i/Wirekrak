#pragma once

#include "lcr/metrics.hpp"
#include "lcr/system/monotonic_clock.hpp"

using namespace lcr::metrics;
using lcr::time_unit;
using lcr::system::monotonic_clock;


namespace flashstrike {
namespace matching_engine {
namespace telemetry {

struct alignas(64) LowLevel {
    alignas(64) stats::size64 partition_size{};
    alignas(64) stats::duration64 allocate_partition{};
    alignas(64) stats::duration64 release_partition{};
    alignas(64) stats::size64 order_id_map_size{};
    alignas(64) stats::operation64 insert_ordid{};
    alignas(64) stats::operation64 remove_ordid{};
    alignas(64) stats::sampler64 order_id_map_probes{};
    alignas(64) stats::size64 order_pool_size{};
    alignas(64) stats::operation64 allocate_order{};
    alignas(64) stats::operation64 release_order{};

    // Constructor
    LowLevel() = default;
    // Disable copy/move semantics
    LowLevel(const LowLevel&) = delete;
    LowLevel& operator=(const LowLevel&) = delete;
    LowLevel(LowLevel&&) noexcept = delete;
    LowLevel& operator=(LowLevel&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(LowLevel& other) const noexcept {
        partition_size.copy_to(other.partition_size);
        allocate_partition.copy_to(other.allocate_partition);
        release_partition.copy_to(other.release_partition);
        order_id_map_size.copy_to(other.order_id_map_size);
        insert_ordid.copy_to(other.insert_ordid);
        remove_ordid.copy_to(other.remove_ordid);
        order_id_map_probes.copy_to(other.order_id_map_probes);
        order_pool_size.copy_to(other.order_pool_size);
        allocate_order.copy_to(other.allocate_order);
        release_order.copy_to(other.release_order);
    }

    // Dump metrics to ostream (human-readable)
    void dump(const std::string& label, std::ostream& os) const noexcept {
        os  << "[" << label << " Metrics] Snapshot:\n";
        os << "-----------------------------------------------------------------\n";
        os << " Partition size     : " << partition_size.str() << "\n";
        os << " Allocate partition : " << allocate_partition.str(time_unit::seconds, time_unit::microseconds) << "\n";
        os << " Release partition  : " << release_partition.str(time_unit::milliseconds, time_unit::microseconds) << "\n";
        os << " --\n";
        os << " Order ID Map size  : " << order_id_map_size.str() << "\n";
        os << " Insert order id    : " << insert_ordid.str(time_unit::seconds, time_unit::microseconds) << "\n";
        os << " Remove order id    : " << remove_ordid.str(time_unit::seconds, time_unit::microseconds) << "\n";
        os << " Order ID Map probes: " << order_id_map_probes.str() << "\n";
        os << " --\n";
        os << " Order Pool size    : " << order_pool_size.str() << "\n";
        os << " Allocate order     : " << allocate_order.str(time_unit::seconds, time_unit::microseconds) << "\n";
        os << " Release order      : " << release_order.str(time_unit::seconds, time_unit::microseconds) << "\n";
        os << "-----------------------------------------------------------------\n";
    }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& prefix, Collector& collector) const noexcept {
        // partition pool
        collector.push_label("subsystem", "partition_pool");
        partition_size.collect(prefix + "_partitionpool_size", collector);
        allocate_partition.collect(prefix + "_partitionpool_allocate", collector);
        release_partition.collect(prefix + "_partitionpool_release", collector);
        collector.pop_label();
        // order ID map
        collector.push_label("subsystem", "order_id_map");
        order_id_map_size.collect(prefix + "_ordermap_size", collector);
        insert_ordid.collect(prefix + "_ordermap_insert", collector);
        remove_ordid.collect(prefix + "_ordermap_remove", collector);
        order_id_map_probes.collect(prefix + "_ordermap_probes", collector);
        collector.pop_label();
        // order pool
        collector.push_label("subsystem", "order_pool");
        order_pool_size.collect(prefix + "_orderpool_size", collector);
        allocate_order.collect(prefix + "_orderpool_allocate", collector);
        release_order.collect(prefix + "_orderpool_release", collector);
        collector.pop_label();
    }
};
// -----------------------------
// Compile-time verification
// -----------------------------
static_assert(sizeof(LowLevel) % 64 == 0, "LowLevel size must be multiple of 64 bytes");
static_assert(alignof(LowLevel) == 64, "LowLevel must be aligned to 64 bytes");
static_assert(offsetof(LowLevel, partition_size) % 64 == 0, "partition_size must start at a cache-line boundary");
static_assert(offsetof(LowLevel, allocate_partition) % 64 == 0, "allocate_partition must start at a cache-line boundary");
static_assert(offsetof(LowLevel, release_partition) % 64 == 0, "release_partition must start at a cache-line boundary");
static_assert(offsetof(LowLevel, order_id_map_size) % 64 == 0, "order_id_map_size must start at a cache-line boundary");
static_assert(offsetof(LowLevel, order_id_map_probes) % 64 == 0, "order_id_map_probes must start at a cache-line boundary");
static_assert(offsetof(LowLevel, order_pool_size) % 64 == 0, "order_pool_size must start at a cache-line boundary");
// -----------------------------


class LowLevelUpdater {
public:
    explicit LowLevelUpdater(LowLevel& metrics)
        : metrics_(metrics)
    {}
    // ------------------------------------------------------------------------

    // Accessors
    inline void on_allocate_partition(uint64_t start_ns) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.partition_size.inc();
        metrics_.allocate_partition.record(start_ns, end_ns);
    }

    inline void on_release_partition(uint64_t start_ns) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.partition_size.dec();
        metrics_.release_partition.record(start_ns, end_ns);
    }

    inline void on_insert_ordid(uint64_t start_ns, bool ok, uint32_t linear_probe_count) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.insert_ordid.record(start_ns, end_ns, ok);
        metrics_.order_id_map_probes.record(linear_probe_count);
        if (ok) {
            metrics_.order_id_map_size.inc();
        } 
    }

    inline void on_remove_ordid(uint64_t start_ns, bool ok, uint32_t linear_probe_count) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.remove_ordid.record(start_ns, end_ns, ok);
        metrics_.order_id_map_probes.record(linear_probe_count);
        if (ok) {
            metrics_.order_id_map_size.dec();
        }
    }

    inline void on_allocate_order(uint64_t start_ns, bool ok) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.allocate_order.record(start_ns, end_ns, ok);
        metrics_.order_pool_size.inc();
    }

    inline void on_release_order(uint64_t start_ns) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.release_order.record(start_ns, end_ns, true);
        metrics_.order_pool_size.dec();
    }

    void dump(const std::string& label, std::ostream& os) const noexcept {
        metrics_.dump(label, os);
    }

private:
    LowLevel& metrics_;
};


} // namespace telemetry
} // namespace matching_engine
} // namespace flashstrike

