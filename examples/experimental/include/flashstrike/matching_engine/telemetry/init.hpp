#pragma once

#include "lcr/metrics.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/format.hpp"

using namespace lcr::metrics;
using lcr::system::monotonic_clock;


namespace flashstrike {
namespace matching_engine {
namespace telemetry {

struct alignas(64) Init {
    // Matching engine metrics
    alignas(64) stats::duration64 create_matching_engine{};
    constant_gauge_u64 matching_engine_memory{0}; // Constant
    // Order book metrics
    alignas(64) stats::duration64 create_order_book{};
    constant_gauge_u64 order_book_memory{0}; // Constant
    // Order pool metrics
    alignas(64) stats::duration64 create_order_pool{};
    constant_gauge_u64 order_pool_capacity{0};  // Constant
    constant_gauge_u64 order_pool_memory{0};  // Constant
    // Order ID map metrics
    alignas(64) stats::duration64 create_order_id_map{};
    constant_gauge_u64 order_id_map_capacity{0}; // Constant
    constant_gauge_u64 order_id_map_memory{0}; // Constant
    // Partition pool metrics
    alignas(64) stats::duration64 create_partition_pool{};
    constant_gauge_u64 partition_pool_capacity{0}; // Constant
    constant_gauge_u64 partition_pool_memory{0}; // Constant
    constant_gauge_u64 partition_size{0}; // Constant
    // Trade ring buffer metrics
    alignas(64) constant_gauge_u64 trades_ring_capacity{0}; // Constant
    constant_gauge_u64 trades_ring_memory{0}; // Constant

    // Constructor
    Init() = default;
    // Disable copy/move semantics
    Init(const Init&) = delete;
    Init& operator=(const Init&) = delete;
    Init(Init&&) noexcept = delete;
    Init& operator=(Init&&) noexcept = delete;

    // Specialized copy method
    inline void copy_to(Init& other) const noexcept {
        // Matching engine
        create_matching_engine.copy_to(other.create_matching_engine);
        matching_engine_memory.copy_to(other.matching_engine_memory);
        // Order book
        create_order_book.copy_to(other.create_order_book);
        order_book_memory.copy_to(other.order_book_memory);
        // Order pool
        create_order_pool.copy_to(other.create_order_pool);
        order_pool_capacity.copy_to(other.order_pool_capacity);
        order_pool_memory.copy_to(other.order_pool_memory);
        // Order ID map
        create_order_id_map.copy_to(other.create_order_id_map);
        order_id_map_capacity.copy_to(other.order_id_map_capacity);
        order_id_map_memory.copy_to(other.order_id_map_memory);
        // Partition pool
        create_partition_pool.copy_to(other.create_partition_pool);
        partition_pool_capacity.copy_to(other.partition_pool_capacity);
        partition_pool_memory.copy_to(other.partition_pool_memory);
        partition_size.copy_to(other.partition_size);
        // Trades ring
        trades_ring_capacity.copy_to(other.trades_ring_capacity);
        trades_ring_memory.copy_to(other.trades_ring_memory);
    }

    // Dump metrics to ostream (human-readable)
    void dump(const std::string& label, std::ostream& os) const noexcept {
        os  << "[" << label << " Metrics] Snapshot:\n";
        os << "-----------------------------------------------------------------\n";
        os << " Create matching engine : " << create_matching_engine.str() << "\n";
        os << " Matching engine memory : " << lcr::format_bytes(matching_engine_memory.load()) << "\n";
        os << " --\n";
        os << " Create order book      : " << create_order_book.str() << "\n";
        os << " Order book memory      : " << lcr::format_bytes(order_book_memory.load()) << "\n";
        os << " --\n";
        os << " Create order pool      : " << create_order_pool.str() << "\n";
        os << " Order pool capacity    : " << order_pool_capacity.load() << "\n";
        os << " Order pool memory      : " << lcr::format_bytes(order_pool_memory.load()) << "\n";
        os << " --\n";
        os << " Create order id map    : " << create_order_id_map.str() << "\n";
        os << " Order id map capacity  : " << order_id_map_capacity.load() << "\n";
        os << " Order id map memory    : " << lcr::format_bytes(order_id_map_memory.load()) << "\n";
        os << " --\n";
        os << " Create partition pool  : " << create_partition_pool.str() << "\n";
        os << " Partition pool capacity: " << partition_pool_capacity.load() << "\n";
        os << " Partition pool memory  : " << lcr::format_bytes(partition_pool_memory.load()) << "\n";
        os << " Partition size         : " << partition_size.load() << "\n";
        os << " --\n";
        os << " Trades ring capacity   : " << trades_ring_capacity.load() << "\n";
        os << " Trades ring memory     : " << lcr::format_bytes(trades_ring_memory.load()) << "\n";
        os << "-----------------------------------------------------------------\n";
    }

    // Metrics collector
    template <typename Collector>
    void collect(const std::string& prefix, Collector& collector) const noexcept {
        collector.push_label("stage", "init");
        // Matching engine config
        create_matching_engine.collect(prefix + "_duration", collector);
        matching_engine_memory.collect(prefix + "_memory_bytes", "Matching engine memory in bytes", collector);
        create_order_book.collect(prefix + "_orderbook_duration", collector);
        order_book_memory.collect(prefix + "_orderbook_memory_bytes", "Order book memory in bytes", collector);
        // Order pool config
        create_order_pool.collect(prefix + "_orderpool_duration", collector);
        order_pool_capacity.collect(prefix + "_orderpool_max_orders", "Order pool capacity (max. active orders)", collector);
        order_pool_memory.collect(prefix + "_orderpool_memory_bytes", "Order pool memory in bytes", collector);
        // Order ID map config
        create_order_id_map.collect(prefix + "_ordermap_duration", collector);
        order_id_map_capacity.collect(prefix + "_ordermap_max_orders", "Order ID map capacity (max. active orders)", collector);
        order_id_map_memory.collect(prefix + "_ordermap_memory_bytes", "Order ID map memory in bytes", collector);
        // Partition pool config
        create_partition_pool.collect(prefix + "_partitionpool_duration", collector);
        partition_pool_capacity.collect(prefix + "_partitionpool_max_partitions", "Partition pool capacity (max. active partitions)", collector);
        partition_pool_memory.collect(prefix + "_partitionpool_memory_bytes", "Partition pool memory in bytes", collector);
        partition_size.collect(prefix + "_partition_size_bytes", "Partition size in bytes", collector);
        // Trades ring config
        trades_ring_capacity.collect(prefix + "_trades_ring_capacity", "Trades ring buffer capacity (number of events)", collector);
        trades_ring_memory.collect(prefix + "_trades_ring_memory_bytes", "Trades ring buffer memory in bytes", collector);
        collector.pop_label(); // init stage
    }
};
// -----------------------------
// Compile-time verification
// -----------------------------
static_assert(sizeof(Init) % 64 == 0, "Init size must be multiple of 64 bytes");
static_assert(alignof(Init) == 64, "Init must be aligned to 64 bytes");
static_assert(offsetof(Init, create_matching_engine) % 64 == 0, "create_matching_engine must start at a cache-line boundary");
static_assert(offsetof(Init, create_order_book) % 64 == 0, "create_order_book must start at a cache-line boundary");
static_assert(offsetof(Init, create_order_pool) % 64 == 0, "create_order_pool must start at a cache-line boundary");
static_assert(offsetof(Init, create_order_id_map) % 64 == 0, "create_order_id_map must start at a cache-line boundary");
static_assert(offsetof(Init, create_partition_pool) % 64 == 0, "create_partition_pool must start at a cache-line boundary");
static_assert(offsetof(Init, trades_ring_capacity) % 64 == 0, "trades_ring_capacity must start at a cache-line boundary");
// -----------------------------


class InitUpdater {
public:
    explicit InitUpdater(Init& metrics)
        : metrics_(metrics)
    {}
    // ------------------------------------------------------------------------

    // Accessors
    inline void on_create_matching_engine(uint64_t start_ns, uint64_t bytes) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.create_matching_engine.record(start_ns, end_ns);
        metrics_.matching_engine_memory.set(bytes);
    }

    inline void on_create_order_book(uint64_t start_ns, uint64_t bytes) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.create_order_book.record(start_ns, end_ns);
        metrics_.order_book_memory.set(bytes);
    }

    inline void on_create_order_pool(uint64_t start_ns, uint64_t max_orders, uint64_t bytes) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.create_order_pool.record(start_ns, end_ns);
        metrics_.order_pool_capacity.set(max_orders);
        metrics_.order_pool_memory.set(bytes);
    }

    inline void on_create_order_id_map(uint64_t start_ns, uint64_t max_orders, uint64_t bytes) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.create_order_id_map.record(start_ns, end_ns);
        metrics_.order_id_map_capacity.set(max_orders);
        metrics_.order_id_map_memory.set(bytes);
    }

    inline void on_create_partition_pool(uint64_t start_ns, uint32_t max_partitions, uint64_t partition_size, uint64_t bytes) const noexcept {
        auto end_ns = monotonic_clock::instance().now_ns();
        metrics_.create_partition_pool.record(start_ns, end_ns);
        metrics_.partition_pool_capacity.set(max_partitions);
        metrics_.partition_pool_memory.set(bytes);
        metrics_.partition_size.set(partition_size);
    }

    inline void on_create_trades_ring(uint64_t capacity, uint64_t bytes) const noexcept {
        metrics_.trades_ring_capacity.set(capacity);
        metrics_.trades_ring_memory.set(bytes);
    }

    void dump(const std::string& label, std::ostream& os) const noexcept {
        metrics_.dump(label, os);
    }

private:
    Init& metrics_;
};


} // namespace telemetry
} // namespace matching_engine
} // namespace flashstrike
