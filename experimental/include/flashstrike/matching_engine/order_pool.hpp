#pragma once

#include <vector>
#include <cstdint>
#include <cassert>

#include "flashstrike/constants.hpp"
#include "flashstrike/types.hpp"
#include "flashstrike/matching_engine/telemetry/init.hpp"
#include "flashstrike/matching_engine/telemetry/low_level.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/memory/footprint.hpp"

using lcr::system::monotonic_clock;


// Use DEBUG define to enable extra checks (double-free detection, valid access assertions).
// These checks help catch bugs during development and testing, but they add overhead.
// In production builds, DEBUG should be undefined to maximize performance.
// The overhead is minimal (a few extra memory writes and checks), but in HFT every microsecond counts.
// Typical overhead in DEBUG mode:
// - allocate(): +1 assert, +1 bool write
// - release(): +2 asserts, +1 bool write
// - get(): +2 asserts

namespace flashstrike {
namespace matching_engine {

// Order structure with intrusive list pointers.
// Represents a single order in memory.
struct Order {
    OrderIdx prev_idx;  // Allow keeping the order inside a doubly-linked intrusive list within a PriceLevel.
    OrderIdx next_idx;  // Same as above.
    OrderIdx next_free; // Intrusive freelist pointer
    OrderId id;         // Unique order ID.
    OrderType type;     // LIMIT or MARKET
    TimeInForce tif;    // GTC, IOC, FOK
    Price price;        // Limit price in ticks (e.g. 10000 = $100.00 if tick=0.01).
    Quantity qty;       // Remaining quantity.
    Quantity filled;    // Filled quantity.
    Side side;          // BID or ASK
    // constructor omitted for speed
};


// Fixed-size intrusive memory pool up to max_orders.
// Preallocated pool of orders with free list management.
// Avoids dynamic memory allocation during order insertions/cancellations.
// Eliminates new/delete per order → avoids heap fragmentation and latency spikes. A must in HFT.
class OrderPool {
public:
    // Constructor initializes the pool, the free list and the free head.
    explicit OrderPool(std::uint64_t max_orders, telemetry::Init& init_metrics, telemetry::LowLevel& low_level_metrics)
        : init_metrics_updater_(init_metrics)
        , low_level_metrics_updater_(low_level_metrics)
    {
        assert(max_orders > 0 && "max_orders must be > 0");
        auto start_ns = monotonic_clock::instance().now_ns();
        pool_.resize(max_orders);
        // Initialize intrusive free list
        for (std::uint64_t i = 0; i < max_orders - 1; ++i) {
            pool_[i].next_free = i + 1;
        }
        pool_[max_orders - 1].next_free = INVALID_INDEX;
        free_head_ = 0; // must start at 0 (first slot)

    #ifdef DEBUG
        allocated_flags_.resize(max_orders, false);
    #endif
        init_metrics_updater_.on_create_order_pool(start_ns, max_orders, memory_usage().total_bytes());
    }

    // Non-copyable
    OrderPool(const OrderPool&) = delete;
    OrderPool& operator=(const OrderPool&) = delete;
    // Non-moveable
    OrderPool(OrderPool&&) noexcept = delete;
    OrderPool& operator=(OrderPool&&) noexcept = delete;

    // Total capacity of the pool
    [[nodiscard]] inline std::uint64_t capacity() const noexcept {
        return static_cast<std::uint64_t>(pool_.size());
    }

    // Current number of allocated orders
    [[nodiscard]] inline std::uint64_t used() const noexcept {
        return used_count_;
    }

    // Current number of free slots
    [[nodiscard]] inline std::uint64_t free_slots() const noexcept {
        return pool_.size() - used_count_;
    }

    // Gives us a free slot.
    // Allocates an order from the pool, returns its index or INVALID_INDEX if full.
    [[nodiscard]] inline OrderIdx allocate() noexcept {
#ifdef ENABLE_FS3_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        if (free_head_ < 0) [[unlikely]] {
#ifdef ENABLE_FS3_METRICS
        low_level_metrics_updater_.on_allocate_order(start_ns, false);
#endif
            return INVALID_INDEX;
        }
        OrderIdx order_idx = free_head_;
        free_head_ = pool_[order_idx].next_free;   // pop from free list
        pool_[order_idx].next_free = INVALID_INDEX; // mark as allocated
        // Clear minimal fields
        pool_[order_idx].prev_idx = INVALID_INDEX;
        pool_[order_idx].next_idx = INVALID_INDEX;
        pool_[order_idx].qty = 0;
#ifdef DEBUG
        assert(!allocated_flags_[order_idx] && "Double allocate detected");   // Ensure it's not already allocated
        allocated_flags_[order_idx] = true;
#endif
#ifdef ENABLE_FS3_METRICS
        low_level_metrics_updater_.on_allocate_order(start_ns, true);
#endif
        ++used_count_;
        return order_idx;
    }

    // Frees a slot.
    // Returns an order to the pool, making its slot available for future allocations.
    inline void release(OrderIdx order_idx) noexcept {
#ifdef ENABLE_FS3_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
#ifdef DEBUG
        assert(order_idx >= 0 && order_idx < (OrderIdx)pool_.size());
        assert(allocated_flags_[order_idx] && "Double free detected");   // Detect double-free
        allocated_flags_[order_idx] = false;
#endif
        pool_[order_idx].next_free = free_head_;
        free_head_ = order_idx;
#ifdef ENABLE_FS3_METRICS
        low_level_metrics_updater_.on_release_order(start_ns);
#endif
        --used_count_;
    }

    // Provides access to an order by index.
    inline Order& get(OrderIdx order_idx) noexcept {
#ifdef DEBUG
        assert(order_idx >= 0 && order_idx < (OrderIdx)pool_.size());
        assert(allocated_flags_[order_idx] && "Accessing freed order");    // Ensure valid access
#endif
        return pool_[order_idx];
    }

    // Provides const access to an order by index.
    inline const Order& get(OrderIdx order_idx) const noexcept {
#ifdef DEBUG
        assert(order_idx >= 0 && order_idx < (OrderIdx)pool_.size());
        assert(allocated_flags_[order_idx] && "Accessing freed order");    // Ensure valid access
#endif
        return pool_[order_idx];
    }

    inline lcr::memory::footprint memory_usage() const noexcept {
        lcr::memory::footprint mf{
            .static_bytes = sizeof(OrderPool),
#ifndef DEBUG
            .dynamic_bytes = pool_.capacity() * sizeof(Order)
#else
            .dynamic_bytes = pool_.capacity() * sizeof(Order) + (sizeof(bool) * (allocated_flags_.capacity()))
#endif
        };
        return mf;
    }

private:
    std::vector<Order> pool_;        // Stores all orders
    OrderIdx free_head_;             // Top of free list stack
    std::uint64_t used_count_{0};   // Used counter

#ifdef DEBUG
    std::vector<bool> allocated_flags_; // Only in debug: track allocations
#endif

    // METRICS --------------------------------------------------------
    telemetry::InitUpdater init_metrics_updater_;
    telemetry::LowLevelUpdater low_level_metrics_updater_;
};

} // namespace matching_engine
} // namespace flashstrike
