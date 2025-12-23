#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <iostream>

#include "flashstrike/matching_engine/order_pool.hpp"
#include "flashstrike/matching_engine/order_id_map.hpp"
#include "flashstrike/matching_engine/partitions.hpp"
#include "flashstrike/matching_engine/telemetry/price_level_store.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/memory/footprint.hpp"
#include "lcr/log/Logger.hpp"

using lcr::system::monotonic_clock;


namespace flashstrike {
namespace matching_engine {

// Comparator for prices based on side (BID/ASK)
// Used to determine better/worse prices at compile time
template<Side SIDE>
struct PriceComparator;

template<>
struct PriceComparator<Side::BID> {
    static constexpr inline bool is_better(Price a, Price b) noexcept {
        return a > b;
    }

    static constexpr inline bool is_worse(Price a, Price b) noexcept {
        return a < b;
    }

    static constexpr inline bool crosses(Price incoming, Price resting) noexcept {
        return incoming >= resting;
    }
};

template<>
struct PriceComparator<Side::ASK> {
    static constexpr inline bool is_better(Price a, Price b) noexcept {
        return a < b;
    }

    static constexpr inline bool is_worse(Price a, Price b) noexcept {
        return a > b;
    }

    static constexpr inline bool crosses(Price incoming, Price resting) noexcept {
        return incoming <= resting;
    }
};

// PriceLevelStore manages multiple partitions for efficient price level access.
// It uses a hash map to store partitions, creating them on demand.

template<Side SIDE>
class PriceLevelStore { 
public:
    PriceLevelStore(OrderPool &order_pool, OrderIdMap &order_idmap, PartitionPool &part_pool, std::uint32_t num_partitions, std::uint32_t partition_bits, telemetry::PriceLevelStore& pls_asks_metrics, telemetry::PriceLevelStore& pls_bids_metrics)
        : order_pool_(order_pool)
        , order_idmap_(order_idmap)
        , part_pool_(part_pool)
        , num_partitions_(num_partitions)
        , partition_bits_(partition_bits)
        , partition_mask_((1ull << partition_bits) - 1)
        , bitmap_words_((num_partitions + BITS_PER_WORD - 1) / BITS_PER_WORD)
        , best_price_(INVALID_PRICE), has_best_(false)
        , metrics_updater_(pls_asks_metrics, pls_bids_metrics)
    {
        assert(num_partitions > 0 && "num_partitions must be > 0");
        assert((num_partitions & (num_partitions - 1)) == 0 && "num_partitions must be a power of two");
        assert(partition_bits > 0 && "partition_bits must be > 0");
        assert(partition_bits_ < (sizeof(Price)*8-1) && "partition_bits too large for Price type");
        active_partitions_ = std::make_unique<Partition*[]>(num_partitions_);
        active_bitmap_ = std::make_unique<std::uint64_t[]>(bitmap_words_);
        std::fill_n(active_partitions_.get(), num_partitions_, nullptr);
        std::fill_n(active_bitmap_.get(), bitmap_words_, 0ull);
    }
    // Non-copyable
    PriceLevelStore(const PriceLevelStore&) = delete;
    PriceLevelStore& operator=(const PriceLevelStore&) = delete;
    // Non-moveable
    PriceLevelStore(PriceLevelStore&&) noexcept = delete;
    PriceLevelStore& operator=(PriceLevelStore&&) noexcept = delete;

    inline bool has_global_best() const { // Check if we have a global best price
        return has_best_;
    }

    inline Price get_global_best() const { // Get the current global best price
        return best_price_;
    }

    inline PriceLevel* get_best_price_level() { // Get the best PriceLevel using best_price_
        if (!has_best_) return nullptr;
        Partition* part = get_partition_(partition_id_(best_price_));
        if (!part) return nullptr;
        return &part->level(offset_in_partition_(best_price_));
    }

    // Get or create a PriceLevel for a given price.
    inline PriceLevel& get_level(Price price) noexcept {
        Partition* part = get_or_create_partition_(partition_id_(price));
        return part->level(offset_in_partition_(price));
    }
    inline const PriceLevel& get_level(Price price) const noexcept {
        const Partition* part = get_partition_(partition_id_(price));
        return part->level(offset_in_partition_(price));
    }

    // Push an order into the appropriate PriceLevel within the store
    inline void insert_order(OrderIdx order_idx, Order &o) noexcept {
#ifdef ENABLE_FS2_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        insert_order_(order_idx, o);
#ifdef ENABLE_FS2_METRICS
        metrics_updater_.on_insert_order<SIDE>(start_ns);
#endif
    }

    // Modify an order's price
    [[nodiscard]] inline bool reprice_order(OrderIdx order_idx, Order &o, Price new_price) noexcept {
#ifdef ENABLE_FS2_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        if(try_to_reprice_order_by_flashstrike_(order_idx, o, new_price)) [[likely]] {
#ifdef ENABLE_FS2_METRICS
            metrics_updater_.on_reprice_order<SIDE>(start_ns);
#endif
            return true;
        }
        remove_order_(order_idx, o);
        o.price = new_price;
        insert_order_(order_idx, o);
#ifdef ENABLE_FS2_METRICS
        metrics_updater_.on_reprice_order<SIDE>(start_ns);
#endif
        return true;
    }

    // Modify an order's quantity
    [[nodiscard]] inline bool resize_order(Order &o, Quantity new_qty) noexcept {
#ifdef ENABLE_FS2_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        // Compute partition and offset and get partition if needed to hold this price level
        PartitionId partid = partition_id_(o.price);
        Partition *part = get_partition_(partid);
        PriceLevelIdx pl_idx = offset_in_partition_(o.price);
        PriceLevel &pl = part->level(pl_idx);
        // Update total quantity
        pl.add_quantity(new_qty - o.qty);
        o.qty = new_qty;
#ifdef ENABLE_FS2_METRICS
        metrics_updater_.on_resize_order<SIDE>(start_ns);
#endif
        return true;
    }

    // Remove an order from the store
    inline void remove_order(OrderIdx order_idx, Order &o) noexcept {
#ifdef ENABLE_FS2_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        remove_order_(order_idx, o);
#ifdef ENABLE_FS2_METRICS
        metrics_updater_.on_remove_order<SIDE>(start_ns);
#endif
    }

    inline lcr::memory::footprint memory_usage() const noexcept {
        lcr::memory::footprint mf{
            .static_bytes = sizeof(PriceLevelStore<SIDE>),
            .dynamic_bytes = 0
        };
        // Add memory usage of each sub-component
        mf.add_dynamic(num_partitions_ * sizeof(Partition*));
        mf.add_dynamic(bitmap_words_ * sizeof(std::uint64_t));
        return mf;
    }

    // Provide const access to active levels
    void debug_dump() const noexcept  {
        // Get active levels
        std::vector<PriceLevel> active;
        for(std::uint32_t word = 0; word < bitmap_words_; ++word) {
            std::uint64_t mask = active_bitmap_[word];
            while(mask) {
                unsigned bit = __builtin_ctzll(mask); // index of lowest set bit
                PartitionId partid = (word << WORD_SHIFT) | bit; // compute partition id
                Partition* part = active_partitions_[partid];
                for(const auto &pl : part->levels()) {
                    if(pl.is_active()) active.push_back(pl);
                }
                mask &= (mask - 1); // clear lowest set bit
            }
        }
        for (const auto& pl : active) {
            if (!pl.is_active() || pl.get_head_idx() == INVALID_INDEX) continue; // only active levels with orders
            std::cout << "  Price=" << pl.get_price() << " total_qty=" << pl.total_quantity() << " orders:\n";
            std::int32_t idx = pl.get_head_idx();
            while (idx != INVALID_INDEX) {
                const Order& o = order_pool_.get(idx);
                std::cout << "    OrderId=" << o.id << " qty=" << o.qty << " filled=" << o.filled << "\n";
                idx = o.next_idx;
            }
        }
    }

private:
    OrderPool &order_pool_;
    OrderIdMap &order_idmap_;
    PartitionPool &part_pool_;
    std::uint32_t num_partitions_;
    std::uint32_t partition_bits_;
    std::uint64_t partition_mask_;
    std::uint32_t bitmap_words_;
    std::unique_ptr<Partition*[]> active_partitions_;
    std::unique_ptr<std::uint64_t[]> active_bitmap_;
    Price best_price_;  // global best price for this store
    bool has_best_;

    telemetry::PriceLevelStoreUpdater metrics_updater_;

    // Private helper methods ----------------------------

    inline Partition* get_partition_(PartitionId partid) noexcept {
        assert(partid < num_partitions_ && "Invalid partition id");
        Partition* part = active_partitions_[partid];
        assert(part && "Partition not found");
        return part;
    }
    inline const Partition* get_partition_(PartitionId partid) const noexcept {
        assert(partid < num_partitions_ && "Invalid partition id");
        Partition* part = active_partitions_[partid];
        assert(part && "Partition not found");
        return part;
    }

    [[nodiscard]] inline Partition* get_or_create_partition_(PartitionId partid) noexcept {
        assert(partid < num_partitions_ && "Invalid partition id");
        Partition* part = active_partitions_[partid];
        if (!part) {
            part = part_pool_.allocate(partid);
            assert(part && "Partition pool exhausted!");
            active_partitions_[partid] = part;
            set_active_(partid); // mark partition as active in bitmap
        }
        return part;
    }

    inline void insert_order_(OrderIdx order_idx, Order &o) noexcept {
        // Get partition for this price level, creating it if needed, and link the order
        PartitionId partid = partition_id_(o.price);
        Partition *part = get_or_create_partition_(partid);
        link_order_(order_idx, o, part);
        // Update best price both at global and partition level
        try_update_global_best_(o.price);
        part->try_update_best<SIDE>(o.price);
    }

    inline void remove_order_(OrderIdx order_idx, Order &o) noexcept {
        PartitionId partid = partition_id_(o.price);
        Partition *part = get_partition_(partid);
        bool partition_best_changed = unlink_order_and_update_partition_(order_idx, o, partid, part);
        WK_TRACE("Popping order:" << o.id << " at price:" << o.price << " qty:" << o.qty << " filled:" << o.filled);
        if (has_best_ && best_price_ == o.price && partition_best_changed) {
            recompute_global_best_();
        }
    }

    inline void try_update_global_best_(Price price) noexcept {
        if (!has_best_) {
            best_price_ = price;
            has_best_ = true;
            return;
        }
        if (PriceComparator<SIDE>::is_better(price, best_price_))
            best_price_ = price;
    }

    // TODO: if profiling shows recompute is still a hotspot, we can add special instruction sets as AVX2/AVX512
    inline void recompute_global_best_() noexcept {
#ifdef ENABLE_FS2_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        // bitmap-based recompute
        Price new_best = INVALID_PRICE;
        // Scan bitmap words for active partitions
        for (std::uint32_t word = 0; word < bitmap_words_; ++word) {
            std::uint64_t mask = active_bitmap_[word];
            while (mask) {
                unsigned bit = __builtin_ctzll(mask); // index of lowest set bit
                PartitionId partid = (word << WORD_SHIFT) | bit; // compute partition id
                Partition* part = active_partitions_[partid];
                if (part) [[likely]] {
                    const Price best_price = part->best_price(); // guaranteed valid price by bitmap
                    // If new_best is invalid, always take the first valid price.
                    if (new_best == INVALID_PRICE || PriceComparator<SIDE>::is_better(best_price, new_best))
                        new_best = best_price;
                }
                mask &= (mask - 1); // clear lowest set bit
            }
        }
        // Update the store global best
        best_price_ = new_best;
        has_best_ = (new_best != INVALID_PRICE);
        WK_TRACE(" -> Recomputed global best " << to_string(SIDE) << " price: " << best_price_);
#ifdef ENABLE_FS2_METRICS
        metrics_updater_.on_recompute_global_best<SIDE>(start_ns);
#endif
    }

    // Attempt a "FlashStrike" order price modification to avoid full partition/global recalculations.
    // Three cases are handled:
    // 1. Same partition, order is NOT the global best: move safely without touching partition/global best.
    // 2. Same partition, order IS the global best: move and update global best only.
    // 3. Cross-partition, order is NOT the best of the old partition: move, update new partition best and global best if needed.
    // Flash strikes #2 and #3 will cover the majority of operations, while #1 is more of a “nice-to-have” edge case.
    // Returns true if a FlashStrike optimization was applied, false otherwise.
    [[nodiscard]] inline bool try_to_reprice_order_by_flashstrike_(OrderIdx order_idx, Order &o, Price new_price) noexcept {
        PartitionId old_pid = partition_id_(o.price);
        PartitionId new_pid = partition_id_(new_price);
        // Must stay in the same partition
        if (old_pid == new_pid) { // The order must be moved inside the partition
            Partition* part = get_partition_(old_pid);
            if (!part) return false; // partition must exist
            if (o.price != best_price_) { // The order is not the best global price, so we can move it without recalculating the global best
                if (PriceComparator<SIDE>::is_worse(new_price, part->best_price())) { // No need to recalculate the partition best
                    // Relink order in the same partition and change the price
                    unlink_order_(order_idx, o, part);
                    WK_TRACE("[FlashStrike#2] Modifying " << to_string(SIDE) << " order " << o.id << ": price from " << o.price << " to " << new_price);
                    o.price = new_price;
                    link_order_(order_idx, o, part);
                    return true;
                }
            }
            else { // (o.price == best_price_) The order is the best global price, but we don't need to recalculate it when it improves, only update the new best price
                if (PriceComparator<SIDE>::is_better(new_price, o.price) && new_price >= part->min_price() && new_price <= part->max_price()) {
                    // Relink order in the same partition and change the price
                    unlink_order_(order_idx, o, part);
                    WK_TRACE("[FlashStrike#1] Modifying " << to_string(SIDE) << " order " << o.id << ": price from " << o.price << " to " << new_price);
                    o.price = new_price;
                    link_order_(order_idx, o, part);
                    // Update the global best since it improved (no need to set the flag has_best_ since it was already true)
                    best_price_ = new_price; // Update global best
                    return true;
                }
            }
        }
        else { // (old_pid != new_pid)
            Partition* old_part = get_partition_(old_pid);
            if (old_part && o.price != old_part->best_price()) { // Order is not the best of the old partition
                Partition* new_part = get_or_create_partition_(new_pid);
                // Relink order from old to new partition
                unlink_order_(order_idx, o, old_part);
                WK_TRACE("[FlashStrike#3] Modifying " << to_string(SIDE) << " order " << o.id << ": price from " << o.price << " to " << new_price);
                o.price = new_price;
                link_order_(order_idx, o, new_part);
                // Update the new partition best and global best if needed
                new_part->try_update_best<SIDE>(new_price);
                try_update_global_best_(new_price);
                return true;
            }
        }
        return false;
    }

    inline const PriceLevel& unlink_order_(OrderIdx order_idx, Order &o, Partition* part) noexcept {
        PriceLevelIdx pl_idx = offset_in_partition_(o.price);
        PriceLevel &pl = part->level(pl_idx);
        // Unlink from old price level
        if(o.prev_idx != INVALID_INDEX) order_pool_.get(o.prev_idx).next_idx = o.next_idx;
        if(o.next_idx != INVALID_INDEX) order_pool_.get(o.next_idx).prev_idx = o.prev_idx;
        if(pl.get_head_idx() == order_idx) pl.set_head_idx(o.next_idx);
        if(pl.get_tail_idx() == order_idx) pl.set_tail_idx(o.prev_idx);
        pl.add_quantity(-o.qty); // Update total quantity
        // If old price level became empty, update its state
        if (pl.get_head_idx() == INVALID_INDEX) { // Only if level became empty
            pl.set_active(false);
            part->clear_active(pl_idx);
        }
        return pl;
    }

    [[nodiscard]] inline bool unlink_order_and_update_partition_(OrderIdx order_idx, Order &o, PartitionId partid, Partition* part) noexcept {
        const PriceLevel& pl = unlink_order_(order_idx, o, part);
        if(!pl.is_active()) {
            if (part->empty()) { // If partition became empty, release it
                (void)partid; // suppress unused variable warning
/*
                part_pool_.release(part);
                active_partitions_[partid] = nullptr;
                clear_active_(partid); // mark partition as inactive in bitmap
*/
                return true; // Partition best changed (partition freed)
            }
            else if (part->best_price() == o.price) { // If the emptied level was the partition best, recompute partition best
#ifdef ENABLE_FS2_METRICS
                auto start_ns = monotonic_clock::instance().now_ns();
#endif
                part->recompute_best<SIDE>();
#ifdef ENABLE_FS2_METRICS
                metrics_updater_.on_recompute_partition_best<SIDE>(start_ns);
#endif
                return true; // Partition best changed (recomputed)
            }
        }
        return false; // Partition best not changed
    }

    inline const PriceLevel& link_order_(OrderIdx order_idx, Order &o, Partition* part) noexcept {
        PriceLevelIdx pl_idx = offset_in_partition_(o.price);
        PriceLevel &pl = part->level(pl_idx);
        o.prev_idx = pl.get_tail_idx();
        o.next_idx = INVALID_INDEX;
        if (o.prev_idx != INVALID_INDEX) 
            order_pool_.get(o.prev_idx).next_idx = order_idx;
        // Update the price level's indices and other fields
        pl.set_tail_idx(order_idx);
        if (pl.get_head_idx() == INVALID_INDEX)    
            pl.set_head_idx(order_idx);
        pl.add_quantity(o.qty);
        // If new price level was inactive, update its state
        if (!pl.is_active()) {
            pl.set_active(true);
            part->set_active(pl_idx);
        }
        return pl;
    }

    // Price to partition/offset helpers
    inline PartitionId partition_id_(Price price) const noexcept { 
        return static_cast<PartitionId>(price >> partition_bits_);
    }

    inline PriceLevelIdx offset_in_partition_(Price price) const noexcept {
        return static_cast<PriceLevelIdx>(price & partition_mask_);
    }

    // Partition activation helpers
    inline void set_active_(PartitionId partid) noexcept {
        active_bitmap_[partid >> WORD_SHIFT] |= (1ULL << (partid & WORD_MASK));
    }

    inline void clear_active_(PartitionId partid) noexcept {
        active_bitmap_[partid >> WORD_SHIFT] &= ~(1ULL << (partid & WORD_MASK));
    }

    inline bool is_active_(PartitionId partid) const noexcept {
        return active_bitmap_[partid >> WORD_SHIFT] & (1ULL << (partid & WORD_MASK));
    }
};

} // namespace matching_engine
} // namespace flashstrike
