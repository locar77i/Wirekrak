#pragma once

#include <vector>
#include <cstdint>
#include <cassert>
#include <algorithm>

#include "flashstrike/constants.hpp"
#include "flashstrike/types.hpp"
#include "flashstrike/matching_engine/telemetry/init.hpp"
#include "flashstrike/matching_engine/telemetry/low_level.hpp"
#include "lcr/system/monotonic_clock.hpp"
#include "lcr/memory/footprint.hpp"

using lcr::system::monotonic_clock;


namespace flashstrike {
namespace matching_engine {

using PartitionId = std::uint32_t;  // Unique partirion identifier
using PartitionIdx = std::int32_t;  // Index in the partition pool
using PriceLevelIdx = std::int32_t; // Index of price level within a partition


// Groups all orders that share the same price.
// Maintains head/tail of the intrusive list of orders at this price level,
// as well as aggregated quantity and active flag.
class PriceLevel {
public:
    inline void set_quantity(Quantity qty) noexcept {
        total_qty_ = qty;
        assert(total_qty_ >= 0 && "total_qty_ must be non-negative after set");
    }
    inline void add_quantity(Quantity qty) noexcept {
        total_qty_ += qty;
        assert(total_qty_ >= 0 && "total_qty_ must be non-negative after addition");
    }
    inline void subtract_quantity(Quantity qty) noexcept {
        total_qty_ -= qty;
        assert(total_qty_ >= 0 && "total_qty_ must be non-negative after subtraction");
    }
    inline Quantity total_quantity() const noexcept { return total_qty_; }

    inline void set_active(bool val) noexcept { active_ = val; }
    inline bool is_active() const noexcept { return active_; }

    // Accessors and mutators
    inline Price get_price() const noexcept { return price_; }
    inline void set_price(Price p) noexcept { price_ = p; }
    inline OrderIdx get_head_idx() const noexcept { return head_idx_; }
    inline void set_head_idx(OrderIdx idx) noexcept { head_idx_ = idx; }
    inline OrderIdx get_tail_idx() const noexcept { return tail_idx_; }
    inline void set_tail_idx(OrderIdx idx) noexcept { tail_idx_ = idx; }

private:
    Price price_;        // Price level in ticks.
    OrderIdx head_idx_;  // Index of the head order in the intrusive list.
    OrderIdx tail_idx_;  // Index of the tail order in the intrusive list.
    Quantity total_qty_; // Total quantity at this price level.
    bool active_;
    // alignment optional
};


// A Partition holds an array of PriceLevels for a range of prices.
class Partition {
public:
    explicit Partition(std::uint64_t partition_size)
        : best_price_(INVALID_PRICE)
        , has_best_(false) 
    {
        levels_.resize(partition_size);
        bitmap_.resize((partition_size + BITS_PER_WORD - 1) / BITS_PER_WORD, 0ull);
    }
    // Non-copyable
    Partition(const Partition&) = delete;
    Partition& operator=(const Partition&) = delete;
    // Moveable
    Partition(Partition&&) noexcept = default;
    Partition& operator=(Partition&&) noexcept = default;

    // Accessors
    inline PriceLevel& level(PriceLevelIdx pl_idx) noexcept { return levels_[pl_idx]; }
    inline const PriceLevel& level(PriceLevelIdx pl_idx) const noexcept { return levels_[pl_idx]; }
    inline const std::vector<PriceLevel>& levels() const noexcept { return levels_; }
    inline Price best_price() const noexcept { return best_price_; }
    inline bool has_best() const noexcept { return has_best_; }

    inline Price min_price() const noexcept { return levels_.front().get_price(); }
    inline Price max_price() const noexcept { return levels_.back().get_price(); }

    inline bool empty() const noexcept {
        for (std::uint64_t word : bitmap_) {
            if (word != 0) return false;
        }
        return true;
    }

    // Initialize partition for a given partid (set prices, reset state)
    inline void initialize_for_partid(PartitionId partid) noexcept {
        Price base_price = partid * levels_.size();
        best_price_ = INVALID_PRICE;
        has_best_ = false;
        std::fill(bitmap_.begin(), bitmap_.end(), 0ull);
        // Initialize all price levels
        for (std::uint64_t i = 0; i < levels_.size(); ++i) {
            PriceLevel &pl = levels_[i];
            pl.set_price(base_price + i);
            pl.set_head_idx(INVALID_INDEX);
            pl.set_tail_idx(INVALID_INDEX);
            pl.set_quantity(0);
            pl.set_active(false);
        }
    }

    // Bit operations for active levels
    inline void set_active(PriceLevelIdx off) noexcept {
        bitmap_[off / BITS_PER_WORD] |= (1ull << (off % BITS_PER_WORD));
    }
    inline void clear_active(PriceLevelIdx off) noexcept {
        bitmap_[off / BITS_PER_WORD] &= ~(1ull << (off % BITS_PER_WORD));
    }
    inline bool is_active(PriceLevelIdx off) const noexcept {
        return bitmap_[off / BITS_PER_WORD] & (1ull << (off % BITS_PER_WORD));
    }

    // Update or recompute best bid/ask
    template<Side SIDE>
    inline void try_update_best(Price price) noexcept {
        if (!has_best_) {
            best_price_ = price;
            has_best_ = true;
            return;
        }
        if constexpr (SIDE == Side::BID) {
            if (price > best_price_) best_price_ = price;
        } else { // ASK
            if (price < best_price_) best_price_ = price;
        }
    }

    template<Side SIDE>
    inline void recompute_best() noexcept {
        if constexpr (SIDE == Side::BID) {
            for (int w = (int)bitmap_.size() - 1; w >= 0; --w) {
                std::uint64_t word = bitmap_[w];
                if (word) {
                    int bit = 63 - __builtin_clzll(word);
                    int pl_idx = w * BITS_PER_WORD + bit;
                    best_price_ = levels_[pl_idx].get_price();
                    has_best_ = true;
                    return;
                }
            }
        }
        else { // ASK
            for (int w = 0; w < (int)bitmap_.size(); ++w) {
                std::uint64_t word = bitmap_[w];
                if (word) {
                    int bit = __builtin_ctzll(word);
                    int pl_idx = w * BITS_PER_WORD + bit;
                    best_price_ = levels_[pl_idx].get_price();
                    has_best_ = true;
                    return;
                }
            }
        }
        best_price_ = INVALID_PRICE;
        has_best_ = false;
    }

    inline lcr::memory::footprint memory_usage() const noexcept {
        return lcr::memory::footprint{
            .static_bytes = sizeof(Partition),
            .dynamic_bytes = levels_.capacity() * sizeof(PriceLevel)
                             + bitmap_.capacity() * sizeof(std::uint64_t)
        };
    }

    void debug_dump() const noexcept {
        std::cout << "Partition best_price=" << best_price_ << " has_best=" << has_best_ << "\n";
        for (int i = 0; i < 3; i++) {
            const PriceLevel &pl = levels_[i];
            std::cout << " Level price=" << pl.get_price() << " qty=" << pl.total_quantity()
                    << " active=" << pl.is_active() << " head=" << pl.get_head_idx()
                    << " tail=" << pl.get_tail_idx() << "\n";
        }
        for (std::uint64_t i = levels_.size() - 3; i < levels_.size(); i++) {
            const PriceLevel &pl = levels_[i];
            std::cout << " Level price=" << pl.get_price() << " qty=" << pl.total_quantity()
                    << " active=" << pl.is_active() << " head=" << pl.get_head_idx()
                    << " tail=" << pl.get_tail_idx() << "\n";
        }
    }

private:
    std::vector<PriceLevel> levels_;
    std::vector<std::uint64_t> bitmap_;   // 1 bit = active level
    Price best_price_;             // best price for this store (BID: max, ASK: min)
    bool has_best_;
};




// The PartitionPool is just a memory manager: give me a slot, I’ll give you a partition. That’s it.
// Tracking what partition belongs to (asset, partid, side) is somebody else’s job (usually the PriceLevelStore or a higher orchestration layer).
// More flexible, because if in the future we want to manage multiple assets, we don’t bake that complexity into the pool.
// - Flexibility: One pool can be shared across multiple instruments.
// - Simplicity: The allocator doesn’t need to know market semantics.
// - Performance: Separation keeps hot-path lookups (price → level) direct and avoids contention inside the allocator.
// So, I think in production-scale order books, the PartitionPool is definitely kept as simple as possible: reserve slots, hand out slots, recycle slots.
class PartitionPool {
public:
    explicit PartitionPool(std::uint32_t num_partitions, std::uint64_t partition_size, telemetry::Init& init_metrics, telemetry::LowLevel& low_level_metrics)
        : init_metrics_updater_(init_metrics)
        , low_level_metrics_updater_{low_level_metrics}
    {
        auto start_ns = monotonic_clock::instance().now_ns();
        partitions_.reserve(num_partitions);
        for (std::uint32_t i = 0; i < num_partitions; ++i) {
            partitions_.emplace_back(partition_size);  // direct in-place construction
        }
        free_list_.resize(num_partitions);
        for (std::uint32_t  i = 0; i < num_partitions; ++i) free_list_[i] = i;
        free_head_ = num_partitions - 1;
        init_metrics_updater_.on_create_partition_pool(start_ns, num_partitions, partition_size, memory_usage().total_bytes());
    }
    // Non-copyable
    PartitionPool(const PartitionPool&) = delete;
    PartitionPool& operator=(const PartitionPool&) = delete;
    // Non-moveable
    PartitionPool(PartitionPool&&) noexcept = delete;
    PartitionPool& operator=(PartitionPool&&) noexcept = delete;

    [[nodiscard]] inline std::uint64_t capacity() const noexcept {
        return static_cast<std::uint64_t>(partitions_.size());
    }

    [[nodiscard]] inline std::uint64_t used() const noexcept {
        return capacity() - free_slots();
    }

    [[nodiscard]] inline std::uint64_t free_slots() const noexcept {
        return static_cast<std::uint64_t>(free_head_ + 1);
    }

    // Allocate a partition and initialize for a specific partid
    [[nodiscard]] inline Partition* allocate(PartitionId partid) noexcept {
#ifdef ENABLE_FS3_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        if (free_head_ < 0) [[unlikely]] return nullptr;
        PartitionIdx part_idx = free_list_[free_head_--];
        Partition &p = partitions_[part_idx];
        p.initialize_for_partid(partid);
#ifdef ENABLE_FS3_METRICS
        low_level_metrics_updater_.on_allocate_partition(start_ns);
#endif
        return &p;
    }

    // Free a partition
    inline void release(Partition* p) noexcept {
#ifdef ENABLE_FS3_METRICS
        auto start_ns = monotonic_clock::instance().now_ns();
#endif
        assert(p >= partitions_.data() && p < partitions_.data() + partitions_.size());
        PartitionIdx part_idx = PartitionIdx(p - partitions_.data()); // compute index
        free_list_[++free_head_] = part_idx;
#ifdef ENABLE_FS3_METRICS
        low_level_metrics_updater_.on_release_partition(start_ns);
#endif
    }

    inline double occupancy_ratio() const noexcept {
        return double(capacity() - (free_head_ + 1)) / double(capacity());
    }

    inline lcr::memory::footprint memory_usage() const noexcept {
        lcr::memory::footprint mf{
            .static_bytes = sizeof(PartitionPool),
            .dynamic_bytes = 0
        };
        // Add memory usage of each sub-component
        mf.add_dynamic(partitions_.capacity() * sizeof(Partition));
        if (!partitions_.empty()) { // All partitions layouts are the same
            mf.add_dynamic(partitions_.size() * partitions_.front().memory_usage().dynamic_bytes);
        }
        mf.add_dynamic(free_list_.capacity() * sizeof(PartitionIdx));
        return mf;
    }

    void debug_dump() const noexcept {
        std::cout << "PartitionPool free_head=" << free_head_ << "\n";
        // Number of free and used partitions
        std::cout << " Free partitions: " << (free_head_ + 1) << "\n";
        std::cout << " Used partitions: " << (int)partitions_.size() - (free_head_ + 1) << "\n";
        // Print all used partitions
        for (int i = free_head_ + 1; i < (int)free_list_.size(); i++) {
            PartitionIdx part_idx = free_list_[i];
            auto &part = partitions_[part_idx];
            if(part.has_best()) {
                std::cout << " Used partition idx=" << part_idx << " best_price=" << part.best_price() << "\n";
            }
        }
    }

private:
    std::vector<Partition> partitions_;    // actual partitions
    std::vector<PartitionIdx> free_list_;  // stack of free partition indices
    PartitionIdx free_head_;

    // METRICS --------------------------------------------------------
    telemetry::InitUpdater init_metrics_updater_;
    telemetry::LowLevelUpdater low_level_metrics_updater_;
};


} // namespace matching_engine
} // namespace flashstrike
