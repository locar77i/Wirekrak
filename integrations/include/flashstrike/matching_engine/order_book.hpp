#pragma once

#include <cstdint>
#include <iostream>

#include "flashstrike/matching_engine/price_level_store.hpp"
#include "flashstrike/matching_engine/telemetry.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/memory/footprint.hpp"
#include "lcr/log/Logger.hpp"

using lcr::system::monotonic_clock;


namespace flashstrike {
namespace matching_engine {

// Main OrderBook class
class OrderBook {
public:
    OrderBook(std::uint64_t max_orders, std::uint32_t num_partitions, std::uint64_t partition_size, std::uint32_t partition_bits, matching_engine::Telemetry& metrics)
        : start_ns_(monotonic_clock::instance().now_ns())
        , order_pool_(max_orders, metrics.init_metrics, metrics.low_level_metrics)
        , order_idmap_(order_pool_.capacity(), metrics.init_metrics, metrics.low_level_metrics)
        , part_pool_(num_partitions, partition_size, metrics.init_metrics, metrics.low_level_metrics)
        , bids_(order_pool_, order_idmap_, part_pool_, num_partitions, partition_bits, metrics.pls_asks_metrics, metrics.pls_bids_metrics)
        , asks_(order_pool_, order_idmap_, part_pool_, num_partitions, partition_bits, metrics.pls_asks_metrics, metrics.pls_bids_metrics)
        , init_metrics_updater_(metrics.init_metrics)
    {
        init_metrics_updater_.on_create_order_book(start_ns_, memory_usage().total_bytes());
    }

    // Accessors
    template<Side SIDE>
    inline PriceLevelStore<SIDE>& get_store() noexcept {
        if constexpr (SIDE == Side::BID) return bids_;
        else return asks_;
    }

    template<Side SIDE>
    inline const PriceLevelStore<SIDE>& get_store() const noexcept {
        if constexpr (SIDE == Side::BID) return bids_;
        else return asks_;
    }

    inline PriceLevelStore<Side::BID> &bids() noexcept { return bids_; }
    inline const PriceLevelStore<Side::BID> &bids() const noexcept { return bids_; }
    inline PriceLevelStore<Side::ASK> &asks() noexcept { return asks_; }
    inline const PriceLevelStore<Side::ASK> &asks() const noexcept { return asks_; }

    inline const OrderPool& order_pool() noexcept { return order_pool_; }
    inline const OrderIdMap& order_id_map() const noexcept { return order_idmap_; }
    inline const PartitionPool& partition_pool() noexcept { return part_pool_; }


    // Order operations
    template<Side SIDE>
    [[nodiscard]] inline OperationStatus insert_order(OrderId orderid, Price price, Quantity qty, Quantity filled, OrderIdx &order_idx_out) noexcept {
        // Allocate order from pool´
        order_idx_out = order_pool_.allocate();
        if (order_idx_out == INVALID_INDEX) [[unlikely]] {
            return OperationStatus::BAD_ALLOC;
        }
        // Update ID map
        if (!order_idmap_.insert(orderid, order_idx_out)) [[unlikely]] {
            order_pool_.release(order_idx_out);
            order_idx_out = INVALID_INDEX;
            return OperationStatus::IDMAP_FULL;
        }
        auto &o = order_pool_.get(order_idx_out);
        // Initialize order
        o.id = orderid;
        o.type = OrderType::LIMIT;
        o.tif = TimeInForce::GTC;
        o.side = SIDE;
        o.price = price;
        o.qty = qty;
        o.filled = filled;
        // Push order into appropriate PriceLevelStore
        get_store<SIDE>().insert_order(order_idx_out, o);
        return OperationStatus::SUCCESS;
    }

    [[nodiscard]] inline OperationStatus insert_order(OrderId orderid, Side side, Price price, Quantity qty, Quantity filled, OrderIdx &order_idx_out) noexcept {
        if (side == Side::BID) {
            return insert_order<Side::BID>(orderid, price, qty, filled, order_idx_out);
        }
        return insert_order<Side::ASK>(orderid, price, qty, filled, order_idx_out);
    }

    [[nodiscard]] inline OperationStatus reprice_order(OrderId orderid, Price new_price, Order** out_order = nullptr) noexcept {
        // If new price is zero, do nothing
        if (new_price == 0) [[unlikely]] {
            return OperationStatus::REJECTED;
        }
        // Get the order from order pool via ID map
        OrderIdx order_idx = order_idmap_.find(orderid);
        if (order_idx == INVALID_INDEX) [[unlikely]] {
            return OperationStatus::NOT_FOUND;
        }
        Order &o = order_pool_.get(order_idx);
        if(out_order) {
            *out_order = &o;
        }
        // If new price is unchanged, do nothing
        if (o.price == new_price) [[unlikely]] {
            return OperationStatus::UNCHANGED;
        }
        // Modify price at the PriceLevelStore level
        bool modified = false;
        if (o.side == Side::BID) {
            modified = bids_.reprice_order(order_idx, o, new_price);
        } else {
            modified = asks_.reprice_order(order_idx, o, new_price);
        }
        if(!modified) [[unlikely]] {
            return OperationStatus::REJECTED;
        }
        return OperationStatus::SUCCESS;
    }

    [[nodiscard]] inline OperationStatus resize_order(OrderId orderid, Quantity new_qty) noexcept {
        // If new quantity is zero, do nothing
        if (new_qty == 0) [[unlikely]] {
            return OperationStatus::REJECTED;
        }
        // Get the order from order pool via ID map
        OrderIdx order_idx = order_idmap_.find(orderid);
        if (order_idx == INVALID_INDEX) [[unlikely]] {
            return OperationStatus::NOT_FOUND;
        }
        Order &o = order_pool_.get(order_idx);
        // If new quantity is unchanged, do nothing
        if (o.qty == new_qty) [[unlikely]] {
            return OperationStatus::UNCHANGED;
        }
        // Modify quantity at the PriceLevelStore level
        bool modified = false;
        if (o.side == Side::BID) {
            modified = bids_.resize_order(o, new_qty);
        } else {
            modified = asks_.resize_order(o, new_qty);
        }
        if(!modified) [[unlikely]] {
            return OperationStatus::REJECTED;
        }
        return OperationStatus::SUCCESS;
    }

    [[nodiscard]] inline OperationStatus remove_order(OrderId orderid) noexcept {
        // Get the order from order pool via ID map
        OrderIdx order_idx = order_idmap_.find(orderid);
        if (order_idx == INVALID_INDEX) [[unlikely]] {
            WK_TRACE("Failed to remove order id " << orderid << ": not found");
            return OperationStatus::NOT_FOUND;
        }
        WK_TRACE("Removing order id " << orderid << " found at index " << order_idx);
        Order &o = order_pool_.get(order_idx);
        // Cancel order in appropriate PriceLevelStore
        if (o.side == Side::BID) {
            bids_.remove_order(order_idx, o);
        } else {
            asks_.remove_order(order_idx, o);
        }
        // Remove from ID map and free order slot
        order_idmap_.remove(orderid);
        order_pool_.release(order_idx);
        return OperationStatus::SUCCESS;
    }

    inline Order &get_order(OrderIdx order_idx) noexcept {
        return order_pool_.get(order_idx);
    }

    inline lcr::memory::footprint memory_usage() const noexcept {
        lcr::memory::footprint mf{
            .static_bytes = sizeof(OrderBook),
            .dynamic_bytes = 0
        };
        // Add memory usage of each sub-component
        mf.add_dynamic(order_pool_);
        mf.add_dynamic(order_idmap_);
        mf.add_dynamic(part_pool_);
        mf.add_dynamic(bids_);
        mf.add_dynamic(asks_);
        return mf;
    }

    void debug_dump() const noexcept {
        std::cout << "=== ORDER BOOK DUMP ===\n";
        // Dump bids and asks
        std::cout << "BIDS:\n";
        bids_.debug_dump();
        std::cout << "ASKS:\n";
        asks_.debug_dump();
        // Dump partition pool status
        part_pool_.debug_dump();
    }

private:
    Timestamp start_ns_;
    OrderPool order_pool_;           // single pool for all orders
    OrderIdMap order_idmap_;          // map orderid -> order pool index
    PartitionPool part_pool_;         // single pool for all partition
    PriceLevelStore<Side::BID> bids_;
    PriceLevelStore<Side::ASK> asks_;

    // METRICS --------------------------------------------------------
    telemetry::InitUpdater init_metrics_updater_;
};

} // namespace matching_engine
} // namespace flashstrike
